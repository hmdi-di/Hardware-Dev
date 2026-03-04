#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <stdlib.h>
#include <config.h>

// Address of main board (MAC Adress: 68:B6:B3:37:F1:64 )
uint8_t main_macAddr[] = {0x68, 0xB6, 0xB3, 0x37, 0xF1, 0x64};
esp_now_peer_info_t peerInfo;

int32_t wifi_channel;
int left, right;
float lx, ly, rx, ry;
int16_t lAcX, lAcY, lAcZ, rAcX, rAcY, rAcZ;

typedef struct message_struct{
    uint8_t type;
    int lhand;
    int rhand;
} message_struct;

typedef struct check_struct{
    uint8_t type;
    bool player;
    bool send;
} check_struct;

volatile check_struct status;
volatile message_struct data_recv;

// ---------------------------------------------------------
// MPU Setting and Comfigure
void setup_mpu(uint8_t address){
    Wire.beginTransmission(address);
    Wire.write(0x6B);
    Wire.write(0);
    Wire.endTransmission(true);
}

void connect_mpu(){
    Wire.begin(4, 5);
    setup_mpu(0x68);
    setup_mpu(0x69);
  
    printf("MPU6050 success initializing.\n");
}

// ---------------------------------------------------------
// WIFI Setting and configure
void connect_wifi(){
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    wifi_channel = 6;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == WIFI_SSID) {
            wifi_channel = WiFi.channel(i);
            break;
        }
    }
    printf("Found target WiFi on Channel: %d\n", wifi_channel);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    printf("wifi success initializing.\n");
}

// ----------------------------------------------------------------------
// ESP-NOW Connect and Recv
void connect_espnow(){
    if (esp_now_init() != ESP_OK){
        printf("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(esp_recv);

    memcpy(peerInfo.peer_addr, main_macAddr, wifi_channel);
    peerInfo.channel = wifi_channel;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        printf("Failed to add peer");
        return;
    }
    printf("peerInfo Channel: %d\n", wifi_channel);
    printf("ESP-NOW success initializing.\n");
}

void esp_recv(const esp_now_recv_info_t * esp_now_info, const uint8_t *dataRecv, int len){
    memcpy((void *)&status, dataRecv, sizeof(status));
}

// -------------------------------------------------------------------------------------------------
// Check Hand player for send to Main ESP32
int check_hand(int x, int y){
    if (abs(x) < 20 && abs(y) < 20)
        return 0;
    else if (y > 60 && y < 120)
        return 1;
    else if (abs(y) < 20 && abs(y) < 180)
        return 2;
    return -1;
}

void read_mpu6050() {
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

    lx = atan2(lAcY, lAcZ) * (180.0 / 3.1415926535);
    ly = atan2(-lAcX, sqrt((long)lAcY * lAcY + (long)lAcZ * lAcZ)) * (180.0 / 3.1415926535);

    rx = atan2(rAcY, rAcZ) * (180.0 / 3.1415926535);
    ry = atan2(-rAcX, sqrt((long)rAcY * rAcY + (long)rAcZ * rAcZ)) * (180.0 / 3.1415926535);

    left  = check_hand(lx, ly);
    right = check_hand(rx, ry);

    // for debug
    // printf("Right: %.2f, %.2f\n", rx, ry);
    // printf("Left:  %.2f, %.2f\n", lx, ly);
    // printf("right: %d | left %d\n\n", left, right);

    data_recv.lhand = left;
    data_recv.rhand = right;

    esp_now_send(main_macAddr, (uint8_t *) &data_recv, sizeof(data_recv));
}

// -----------------------------------------------------------------------------
// Main Program
void setup(){
    Serial.begin(115200);

    connect_wifi();
    connect_espnow();
    connect_mpu();

    data_recv.type = '1';
    status.player = false;
    while (!status.player){
        // for debug
        // printf("ESP32 main not found!\n");

        esp_now_send(main_macAddr, (uint8_t *) &data_recv, sizeof(data_recv));
        delay(40);
    }
    printf("ESP32 main found.\n");

    data_recv.type = '0';
    esp_now_send(main_macAddr, (uint8_t *) &data_recv, sizeof(data_recv));
    printf("setup is success.\n");
}

void loop(){
    while (status.send){
        read_mpu6050();
        delay(150);
    }

    // for debug
    // read_sensors();

    delay(150);
}