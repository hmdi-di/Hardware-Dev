#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 lmpu; 
Adafruit_MPU6050 rmpu;

// Address of main board (MAC Adress: 68:B6:B3:37:F1:64 )
uint8_t main_macAddr[] = {0x68, 0xB6, 0xB3, 0x37, 0xF1, 0x64};
esp_now_peer_info_t peerInfo;

typedef struct message{
    uint8_t type;
    int id;
    float lx;
    float ly;
    float lz;
    float rx;
    float ry;
    float rz;
} message;

typedef struct check{
    uint8_t type;
    int id;
    bool player;
    bool send;
} check;

message data_recv;
check status;

void esp_recv(const esp_now_recv_info_t * esp_now_info, const uint8_t *dataRecv, int len){
    memcpy(&status, dataRecv, sizeof(status));
}

void connect_espnow(){
    if (esp_now_init() != ESP_OK){
        printf("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(esp_recv);

    memcpy(peerInfo.peer_addr, main_macAddr, 6);
    peerInfo.channel = 6;  
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        printf("Failed to add peer");
        return;
    }
}

void setup_mpu(){
    // right mpu ต่อ VCC
    Wire.begin(4, 5);
    if (!rmpu.begin(0x68)) {
        printf("Failed to find Right MPU6050\n");
        while (1) {
            delay(10);
        }
    }
    printf("Right MPU6050 Found!\n");

    // left mpu ต่อ GND
    if (!lmpu.begin(0x69)) {
        printf("Failed to find Left MPU6050\n");
        while (1) {
            delay(10);
        }
    }
    printf("Left MPU6050 Found!\n");
}

void read_sensors() {
    sensors_event_t la, lg, ltemp;
    sensors_event_t ra, rg, rtemp;

    lmpu.getEvent(&la, &lg, &ltemp);
    rmpu.getEvent(&ra, &rg, &rtemp);
    
    data_recv.lx = lg.gyro.x*(180/3.1415926535);
    data_recv.ly = lg.gyro.y*(180/3.1415926535);
    data_recv.lz = lg.gyro.z*(180/3.1415926535);
    data_recv.rx = rg.gyro.x*(180/3.1415926535);
    data_recv.ry = rg.gyro.y*(180/3.1415926535);
    data_recv.rz = rg.gyro.z*(180/3.1415926535);

    printf("Right: %.2f, %.2f, %.2f\n", data_recv.rx, data_recv.ry, data_recv.rz);
    printf("Left: %.2f, %.2f, %.2f\n", data_recv.lx, data_recv.ly, data_recv.lz);
}

void setup(){
    Serial.begin(115200);

    setup_mpu();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    connect_espnow();

    data_recv.type = 1;
    status.player = false;
    status.send = false;
    while (!status.player){
        printf("A Can't find B\n");
        esp_now_send(main_macAddr, (uint8_t *) &data_recv, sizeof(data_recv));
        delay(200);
    }
    printf("ESP32 player Found!\n");
}

void loop(){
    while (status.send){
        read_sensors();
        delay(100);
    }
    delay(250);
}