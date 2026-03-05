#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <config.h>

#define TOPIC_PREFIX   "game"

#define RED_GPIO        42
#define YELLOW_GPIO     41
#define GREEN_GPIO      40

#define RED_1           0   
#define GREEN_1         0
#define RED_2           0          
#define GREEN_2         0
#define RED_3           0          
#define GREEN_3         0
#define RED_4           0         
#define GREEN_4         0
#define RED_5           0            
#define GREEN_5         0
#define RED_6           0            
#define GREEN_6         0

#define JOY_X_PIN       4
#define JOY_Y_PIN       5
#define JOY_SW_PIN      6
#define TFT_CS          10
#define TFT_RST         9
#define TFT_DC          8
#define BUZZER_PIN      15

uint8_t player_macAddr[] = {0x68, 0xB6, 0xB3, 0x38, 0x02, 0x4C};
esp_now_peer_info_t peerInfo;

WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

typedef struct message_struct{
    uint8_t type = 1;
    int lhand;
    int rhand;
} message_struct;

typedef struct status_struct{
    uint8_t type = 2;
    bool esp_main;
    bool esp_player;
    bool send_recv;
} status_struct;

volatile status_struct  global_status;
volatile message_struct global_data;

bool rcheck, lcheck;

int spd_total = 0, spd_win = 0, spd_lose = 0;
int mem_total = 0, mem_win = 0, mem_lose = 0;

int times_played_spd = 0;
int times_played_mem = 0;
int times_played_diff[3] = {0, 0, 0};

enum State { SELECT_GAME, SELECT_DIFF, PLAYING, GAME_OVER };
State currentState = SELECT_GAME;

int selectedGame = 1;
int selectedDiff = 1;
unsigned long lastJoyMove = 0;
unsigned long lastDebounceTime = 0;

void playStartSound() {
    tone(BUZZER_PIN, 1200, 150); // เสียงสูง 1200Hz นาน 150ms (เสียงติ๊ง ครั้งเดียว)
    delay(150);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, HIGH); // ดับเสียง
}

void playCorrectSound() {
    tone(BUZZER_PIN, 1200, 100); 
    delay(150);                  
    tone(BUZZER_PIN, 1600, 150); 
    delay(150);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, HIGH); // ดับเสียง
}

void playWrongSound() {
    tone(BUZZER_PIN, 300, 400);  
    delay(400);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, HIGH); // ดับเสียง
}

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

    printf("MQTT broker connected.\n");
}

void publishGameStats() {
    if (!mqtt.connected()) 
        connect_mqtt(); 

    float spd_pct = (spd_total > 0) ? ((float)spd_win / spd_total) * 100.0 : 0.0;
    float mem_pct = (mem_total > 0) ? ((float)mem_win / mem_total) * 100.0 : 0.0;

    String most_game = "None";
    if (times_played_spd > times_played_mem) most_game = "Speed Game";
    else if (times_played_mem > times_played_spd) most_game = "Memory Game";
    else if (times_played_spd > 0) most_game = "Both Equal";

    String most_diff = "Easy";
    int max_diff = times_played_diff[0];
    if (times_played_diff[1] > max_diff) { most_diff = "Medium"; max_diff = times_played_diff[1]; }
    if (times_played_diff[2] > max_diff) { most_diff = "Hard"; }
    if (max_diff == 0) most_diff = "None";

    mqtt.publish(TOPIC_PREFIX "/speed/rounds", String(spd_total).c_str());
    mqtt.publish(TOPIC_PREFIX "/speed/win", String(spd_win).c_str());
    mqtt.publish(TOPIC_PREFIX "/speed/lose", String(spd_lose).c_str());
    mqtt.publish(TOPIC_PREFIX "/speed/win_percent", String(spd_pct, 2).c_str());

    mqtt.publish(TOPIC_PREFIX "/memory/rounds", String(mem_total).c_str());
    mqtt.publish(TOPIC_PREFIX "/memory/win", String(mem_win).c_str());
    mqtt.publish(TOPIC_PREFIX "/memory/lose", String(mem_lose).c_str());
    mqtt.publish(TOPIC_PREFIX "/memory/win_percent", String(mem_pct, 2).c_str());

    mqtt.publish(TOPIC_PREFIX "/history/most_played_game", most_game.c_str());
    mqtt.publish(TOPIC_PREFIX "/history/most_played_diff", most_diff.c_str());

    printf(">>> MQTT Stats Published Successfully! <<<\n");
}

void esp_recv(const esp_now_recv_info_t * esp_now_info, const uint8_t *dataRecv, int len){
    uint8_t incoming_type = dataRecv[0];
    if (incoming_type == 1) memcpy((void *)&global_data, dataRecv, sizeof(global_data));
    else if (incoming_type == 2) memcpy((void *)&global_status, dataRecv, sizeof(global_status));
}

void connect_espnow(){
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(esp_recv);

    int32_t wifi_channel = WiFi.channel();
    memcpy(peerInfo.peer_addr, player_macAddr, 6);
    peerInfo.channel = wifi_channel;  
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void all_low(){
    digitalWrite(RED_1, 0);
    digitalWrite(RED_2, 0);
    digitalWrite(RED_3, 0);
    digitalWrite(RED_4, 0);
    digitalWrite(RED_5, 0);
    digitalWrite(RED_6, 0);
    digitalWrite(GREEN_1, 0);
    digitalWrite(GREEN_2, 0);
    digitalWrite(GREEN_3, 0);
    digitalWrite(GREEN_4, 0);
    digitalWrite(GREEN_5, 0);
    digitalWrite(GREEN_6, 0);
}

void RED_active(int side, int n){
    if (side == 0){ 
        if (n == 0)
            digitalWrite(RED_1, 1);
        else if (n == 1)
            digitalWrite(RED_2, 1);
        else if (n == 2)
            digitalWrite(RED_3, 1);
    }else if (side == 1){   
        if (n == 0)
            digitalWrite(RED_4, 1);
        else if (n == 1)
            digitalWrite(RED_5, 1);
        else if (n == 2)
            digitalWrite(RED_6, 1);
    } 
}

void GREEN_active(int side, int n){
    if (side == 0){ 
        if (n == 0)
            digitalWrite(GREEN_1, 1);
        else if (n == 1)
            digitalWrite(GREEN_2, 1);
        else if (n == 2)
            digitalWrite(GREEN_3, 1);
    }else if (side == 1){   
        if (n == 0)
            digitalWrite(GREEN_4, 1);
        else if (n == 1)
            digitalWrite(GREEN_5, 1);
        else if (n == 2)
            digitalWrite(GREEN_6, 1);
    } 
}

int game(int n, int times, int sleep_time, int delay_time){
    int w = 0, l = 0;
    int lrand[n], rrand[n];

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 60);
    tft.setTextColor(ST77XX_ORANGE);
    tft.setTextSize(2);
    if (n > 1) tft.println("MEMORIZE!"); 
    else tft.println("READY!");          

    for (int i=0;i < n; i++){
        lrand[i] = esp_random() % 3;
        rrand[i] = esp_random() % 3;

        all_low();
        RED_active(0, lrand[i]);
        RED_active(1, rrand[i]);
        delay(delay_time);
    }

    delay(sleep_time);

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(5, 60);
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(2);
    tft.println("YOUR TURN");

    for (int i=0; i < n; i++){
        int j=0;
        lcheck = 0;
        rcheck = 0;
        do {
            lcheck = (lrand[i] == global_data.lhand);
            rcheck = (rrand[i] == global_data.rhand);
            j++;
            delay(200);
        } while (j < times && !(rcheck && lcheck));

        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(15, 60);
        tft.setTextSize(2);
        
        if (rcheck && lcheck){
            all_low();
            GREEN_active(0, lrand[i]);
            GREEN_active(1, rrand[i]);
            tft.setTextColor(ST77XX_GREEN);
            tft.println("CORRECT!");
            playCorrectSound();
            w++;
        }else{
            tft.setTextColor(ST77XX_RED);
            tft.setCursor(30, 60); 
            tft.println("WRONG!");
            playWrongSound();
            l++;
        }
        delay(1000); 
        tft.fillScreen(ST77XX_BLACK);

        all_low();
    }
    return w;
}

void drawBar(int y, const char* text, uint16_t themeColor, bool selected) {
    if (selected) {
        tft.fillRect(10, y, 108, 22, themeColor);
        tft.setTextColor(ST77XX_BLACK);
    } else {
        tft.fillRect(10, y, 108, 22, ST77XX_BLACK);
        tft.drawRect(10, y, 108, 22, themeColor);
        tft.setTextColor(themeColor);
    }
    tft.setTextSize(1);
    tft.setCursor(18, y + 7);
    tft.print(text);
}

void printMenuGame() { 
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(12, 15); 
    tft.setTextColor(ST77XX_WHITE); 
    tft.setTextSize(1);
    tft.println("=== SELECT GAME ===");
    
    drawBar(40, "Speed Game", ST77XX_CYAN, selectedGame == 1);
    drawBar(70, "Memory Game", ST77XX_CYAN, selectedGame == 2);
}

void printMenuDiff() { 
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 10); 
    tft.setTextColor(ST77XX_WHITE); 
    tft.setTextSize(1);
    tft.println("=== DIFFICULTY ===");
    
    drawBar(30, "Easy", ST77XX_GREEN, selectedDiff == 1);
    drawBar(55, "Medium", ST77XX_YELLOW, selectedDiff == 2);
    drawBar(80, "Hard", ST77XX_RED, selectedDiff == 3);
    drawBar(105, "<- Back", ST77XX_WHITE, selectedDiff == 4);
}

void setup() {
    // 1. บังคับปิดเสียง Buzzer ตั้งแต่บรรทัดแรกสุดของโปรแกรม!!
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH); 

    Serial.begin(115200);

    tft.initR(INITR_BLACKTAB); 
    tft.setRotation(0); 
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(5, 50); tft.setTextColor(ST77XX_WHITE); tft.println("Initializing...");
    
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    pinMode(RED_1, OUTPUT);
    pinMode(RED_2, OUTPUT);
    pinMode(RED_3, OUTPUT);
    pinMode(RED_4, OUTPUT);
    pinMode(RED_5, OUTPUT);
    pinMode(RED_6, OUTPUT);
    pinMode(GREEN_1, OUTPUT);
    pinMode(GREEN_2, OUTPUT);
    pinMode(GREEN_3, OUTPUT);
    pinMode(GREEN_4, OUTPUT);
    pinMode(GREEN_5, OUTPUT);
    pinMode(GREEN_6, OUTPUT);

    connect_wifi();
    connect_mqtt();
    connect_espnow();

    pinMode(RED_GPIO, OUTPUT);
    pinMode(GREEN_GPIO, OUTPUT);
    digitalWrite(RED_GPIO, 1);
    digitalWrite(GREEN_GPIO, 0);

    while (!global_status.esp_player){ delay(40); }

    global_status.esp_main = 1;
    esp_now_send(player_macAddr, (uint8_t *) &global_status, sizeof(global_status));

    digitalWrite(RED_GPIO, 0);
    digitalWrite(GREEN_GPIO, 1);
    printMenuGame();
}

void loop(){
    if (!mqtt.connected()) connect_mqtt();
    mqtt.loop();

    int joyY = analogRead(JOY_Y_PIN);
    bool swPressed = (digitalRead(JOY_SW_PIN) == LOW);

    if (millis() - lastJoyMove > 300) {
        if (joyY < 1000) {
            if (currentState == SELECT_GAME) { selectedGame = (selectedGame == 1) ? 2 : 1; printMenuGame(); }
            else if (currentState == SELECT_DIFF) { selectedDiff = (selectedDiff > 1) ? selectedDiff - 1 : 4; printMenuDiff(); }
            lastJoyMove = millis();
        } 
        else if (joyY > 3000) {
            if (currentState == SELECT_GAME) { selectedGame = (selectedGame == 2) ? 1 : 2; printMenuGame(); } 
            else if (currentState == SELECT_DIFF) { selectedDiff = (selectedDiff < 4) ? selectedDiff + 1 : 1; printMenuDiff(); }
            lastJoyMove = millis();
        }
    }

    if (swPressed && (millis() - lastDebounceTime > 500)) {
        lastDebounceTime = millis();
        
        if (currentState == SELECT_GAME) {
            currentState = SELECT_DIFF;
            selectedDiff = 1;
            printMenuDiff();
        } 
        else if (currentState == SELECT_DIFF) {
            if (selectedDiff == 4) {
                currentState = SELECT_GAME;
                printMenuGame();
            } else {
                currentState = PLAYING;
                tft.fillScreen(ST77XX_BLACK);
                tft.setCursor(15, 60); 
                tft.setTextColor(ST77XX_CYAN); tft.setTextSize(2);
                tft.println("START..."); 
                tft.setTextSize(1);
                playStartSound();
            }
        }
        else if (currentState == GAME_OVER) {
            currentState = SELECT_GAME;
            printMenuGame();
        }
    }

    if (currentState == PLAYING) {
        global_status.send_recv = true;
        esp_now_send(player_macAddr, (uint8_t *) &global_status, sizeof(global_status));
        delay(100);

        int local_win = 0, local_lose = 0;
        int p_n = 1, p_times = 25, p_sleep = 0, p_delay = 200;

        times_played_diff[selectedDiff - 1]++;

        if (selectedGame == 1) {
            times_played_spd++; 
            int i_max = 10;
            if (selectedDiff == 1)      { p_times = 35; p_delay = 500; }
            else if (selectedDiff == 2) { p_times = 25; p_delay = 300; }
            else if (selectedDiff == 3) { p_times = 15; p_delay = 150; }

            for (int i=0; i < i_max; i++){
                if (game(1, p_times, 0, p_delay)) local_win++;
                else local_lose++;
                delay(700); 
            }
            spd_win += local_win;
            spd_lose += local_lose;
            spd_total += (local_win + local_lose);
        } 
        else if (selectedGame == 2) {
            times_played_mem++; 
            if (selectedDiff == 1)      { p_n = 3; p_times = 35; p_sleep = 2500; p_delay = 1500; }
            else if (selectedDiff == 2) { p_n = 5; p_times = 25; p_sleep = 2000; p_delay = 1000; }
            else if (selectedDiff == 3) { p_n = 7; p_times = 15; p_sleep = 1000; p_delay = 500;  }

            local_win = game(p_n, p_times, p_sleep, p_delay);
            local_lose = p_n - local_win;
            
            mem_win += local_win;
            mem_lose += local_lose;
            mem_total += (local_win + local_lose);
        }

        global_status.send_recv = false;
        esp_now_send(player_macAddr, (uint8_t *) &global_status, sizeof(global_status));
        
        currentState = GAME_OVER;

        publishGameStats();
        
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(10, 20); tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2); tft.println("GAME OVER");
        tft.setTextSize(1);
        tft.setCursor(10, 60); tft.setTextColor(ST77XX_GREEN); tft.print("WIN : "); tft.println(local_win);
        tft.setCursor(10, 80); tft.setTextColor(ST77XX_RED); tft.print("LOSE: "); tft.println(local_lose);
        tft.setCursor(10, 110); tft.setTextColor(ST77XX_YELLOW); tft.println("Press button to");
        tft.setCursor(10, 125); tft.println("return to menu");
    }
    delay(200);
}