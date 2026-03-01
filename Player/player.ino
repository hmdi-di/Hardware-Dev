#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Address of main board (MAC Adress: 68:B6:B3:37:F1:64 )
uint8_t main_macAddr[] = {0x68, 0xB6, 0xB3, 0x37, 0xF1, 0x64};
esp_now_peer_info_t peerInfo;

typedef struct message_struct{
    uint8_t type;   
    int id;
    float lx;
    float ly;
    // float lz;
    float rx;
    float ry;
    // float rz;
} message_struct;

typedef struct check_struct{
    uint8_t type;
    int id;
    bool player;
    bool send;
} check_struct;

message_struct data_recv;
check_struct status;

int16_t lAcX, lAcY, lAcZ, rAcX, rAcY, rAcZ;

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
    Wire.begin(4, 5);

    // rmpu
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);
    Wire.write(0);
    Wire.endTransmission(true);

    // lmpu
    Wire.beginTransmission(0x69);
    Wire.write(0x6B);
    Wire.write(0);
    Wire.endTransmission(true);
  
    Serial.println("MPU6050 Initialized!");
}

void read_sensors() {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);  // เริ่มดึงข้อมูลจาก Register ของ Accel X
    Wire.endTransmission(false);
    
    Wire.requestFrom((uint16_t)(0x68), (uint8_t)6, true);
    lAcX = Wire.read()<<8 | Wire.read();  
    lAcY = Wire.read()<<8 | Wire.read();  
    lAcZ = Wire.read()<<8 | Wire.read();

    Wire.beginTransmission(0x69);
    Wire.write(0x3B); 
    Wire.endTransmission(false);
    
    Wire.requestFrom((uint16_t)0x69, (uint8_t)6, true);
    rAcX = Wire.read()<<8 | Wire.read();  
    rAcY = Wire.read()<<8 | Wire.read();  
    rAcZ = Wire.read()<<8 | Wire.read();

    float lx = atan2(lAcY, lAcZ) * (180.0 / 3.1415926535);
    float ly = atan2(-lAcX, sqrt((long)lAcY * lAcY + (long)lAcZ * lAcZ)) * (180.0 / 3.1415926535);

    float rx = atan2(rAcY, rAcZ) * (180.0 / 3.1415926535);
    float ry = atan2(-rAcX, sqrt((long)rAcY * rAcY + (long)rAcZ * rAcZ)) * (180.0 / 3.1415926535);
    
    data_recv.lx = lx;
    data_recv.ly = ly;

    data_recv.rx = rx;
    data_recv.ry = ry;

    printf("Right: %.2f, %.2f, %.2f\n", data_recv.rx, data_recv.ry);
    printf("Left: %.2f, %.2f, %.2f\n", data_recv.lx, data_recv.ly);

    esp_now_send(main_macAddr, (uint8_t *) &data_recv, sizeof(data_recv));
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
        delay(200);
    }
    delay(200);
}