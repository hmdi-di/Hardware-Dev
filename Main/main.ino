#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <config.h>

#define RED_GPIO       42
#define YELLOW_GPIO    41
#define GREEN_GPIO     40

int round = 0;

// Address of player board (MAC Adress: 68:B6:B3:38:02:4C)
uint8_t player_macAddr[] = {0x68, 0xB6, 0xB3, 0x38, 0x02, 0x4C};
esp_now_peer_info_t peerInfo;

WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);

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
bool player = false;

void connect_wifi(){
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    printf("WiFi MAC address is %s\n", WiFi.macAddress().c_str());
    printf("Connecting to WiFi %s.\n", WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED) {
        printf(".");
        fflush(stdout);
        delay(500);
    }
    printf("\nWiFi connected.\n");
}

void connect_mqtt(){
    printf("Connecting to MQTT broker at %s.\n", MQTT_BROKER);

    if (!mqtt.connect("", MQTT_USER, MQTT_PASS)) {
        printf("Failed to connect to MQTT broker.\n");
        for (;;) {}
    }

    // add for publish and subscribe.

    printf("MQTT broker connected.\n");
}

void esp_recv(const esp_now_recv_info_t * esp_now_info, const uint8_t *dataRecv, int len){
    memcpy(&data_recv, dataRecv, sizeof(data_recv));
    if (data_recv.type == 1){
        printf("ESP32 Ready to play");
        player = true;
    }
}

void connect_espnow(){
    if (esp_now_init() != ESP_OK){
        printf("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(esp_recv);

    memcpy(peerInfo.peer_addr, player_macAddr, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        printf("Failed to add peer");
        return;
    }
}

int check_hand(int lx, int ly){
    // 0 = มือหงาย
    // 1 = มือตั้ง
    // 2 = มือคว้ำ

    if (ly > 0 && ly < 20) return 0;
    else if (ly > 70 && ly < 110) return 1;
    else if (ly > 160 && ly < 200) return 2;
}

bool speed_game(){
    int left_random = esp_random() % 3;
    int right_random = esp_random() % 3;

    // การแสดงผล

    
    delay(1000);
}

void setup() {
    Serial.begin(115200);

    // connect all service
    connect_wifi();
    connect_mqtt();
    connect_espnow();

    pinMode(RED_GPIO, OUTPUT);

    // check player board is ready
    while (!player){
        printf("find B (player)\n")
        delay(200);
    }

    status.player = true;
    esp_now_send(player_macAddr, (uint8_t *) &status, sizeof(status));
}

void loop(){
    mqtt.loop();
    
    int mode=1, win=0, lose=0, total;
    switch (mode){
        case (1): {
            for (int i=0; i < 5;i++){
                if (speed_game()){
                    win++;
                }else{
                    lose++;
                }
            }
            total = win+lose;
            break;
        }
        case (2): {

        }
    }

}
