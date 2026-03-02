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
#define TOPIC_ROUND             TOPIC_PREFIX "/game/round"
#define TOPIC_SPEED_WIN         TOPIC_PREFIX "/game/speed/win"
#define TOPIC_SPEED_LOSE        TOPIC_PREFIX "/game/speed/lose"

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
bool right, left;
int game_round = 0;
int speed_win_total = 0;
int speed_lose_total = 0;

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
    mqtt.setCallback(mqtt_callback);

    printf("MQTT broker connected.\n");
}
void mqtt_callback(char* topic, byte* payload, unsigned int length) {

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

int check_player_hand(int x, int y){
    // 0 = มือหงาย
    // 1 = มือตั้ง
    // 2 = มือคว้ำ

    if (y < 20) 
        return 0;
    else if (70 < y && y < 110) 
        return 1;
    else if (-160 < x && x < -180) 
        return 2;
    return -1;
}

bool speed_game(){
    int left_rand  = esp_random() % 3;
    int right_rand = esp_random() % 3;

    printf("Right: %d\n", right_rand);
    printf("Left: %d\n", left_rand);

    // การแสดงผล


    int i = 0;
    left  = false;
    right = false;
    do {
        left =  (left_rand  == check_player_hand(data_recv.lx, data_recv.ly));
        right = (right_rand == check_player_hand(data_recv.rx, data_recv.ry));

        i++;

        delay(40);
    } while (i < 50 && !(right && left));

    printf("result: %d", (left && right));

    if (left && right)
        return true;
    else
        return false;
}

bool memory_game(){
    
}

void publishInt(const char* topic, int value) {
    String payload(value);
    mqtt.publish(topic, payload.c_str());
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
        printf("find B (player)\n");

        delay(200);
    }

    status.player = true;
    status.send = false;
    esp_now_send(player_macAddr, (uint8_t *) &status, sizeof(status));
}

void loop(){
    mqtt.loop();

    // mode 1 speed game แค่ 5 รอบ
    // mode 2 speed game ไปเรื่อยๆ
    int mode=1;

    
    int i_max=5, delay_game=400;
    int win=0, lose=0, total;
    switch (mode){
        case (1): {
            status.send = true;
            esp_now_send(player_macAddr, (uint8_t *) &status, sizeof(status));
            delay(100);

            for (int i=0; i < i_max;i++){
                if (speed_game())
                    win++;
                else
                    lose++;

                delay(delay_game);
            }
            total = (win + lose);
            speed_win_total += win;
            speed_lose_total += lose;

            status.send = false;
            esp_now_send(player_macAddr, (uint8_t *) &status, sizeof(status));
            break;
        }
        case (2): {

        }
    }

    game_round++;

    publishInt(TOPIC_ROUND, game_round);
    publishInt(TOPIC_SPEED_WIN, speed_win_total);
    publishInt(TOPIC_SPEED_LOSE, speed_lose_total);

    delay(100);
}
