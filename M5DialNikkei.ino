#include <M5Dial.h>

#define WIFI_SSID     "xxxxx"
#define WIFI_PASSWORD "xxxxxxxxxx"

#include <WiFi.h>
#include "ArduinoJson.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// 日経平均を取得
const char *apiServer = "https://query1.finance.yahoo.com/v8/finance/chart/%5EN225?interval=1d";

char buf[100];
int sx, sy;
double nowyen = 0.0;  // 最新のNikkei
double oldyen;        // ひとつ前のNikkei
int redraw = 0;  // Nikkei更新で表示更新
int bhour = 0;   // Nikkei取得時間
int bmin = 0;    // Nikkei取得時間
int bsec = 0;    // Nikkei取得時間
char date[100];
int bup = 0;     // 値上がり
int bdown = 0;   // 値下がり

// 初期設定
void setup() {
	// put your setup code here, to run once:
	
	auto cfg = M5.config();
	M5Dial.begin(cfg, true, true);
	
	M5Dial.Display.fillScreen(BLACK);
	M5Dial.Display.setTextColor(GREEN, BLACK);
	M5Dial.Display.setTextDatum(middle_center);
	M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
	M5Dial.Display.setTextSize(0.5);
	
	sx = M5Dial.Display.width() / 2;
	sy = M5Dial.Display.height() / 2;
	
	// WiFi接続
	M5Dial.Display.fillScreen(BLACK);
	M5Dial.Display.drawString("WiFi connecting...", sx, sy);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	int tout = 0;
	while(WiFi.status() != WL_CONNECTED){
		delay(500);
		tout++;
		if(tout > 10){  // 5秒でタイムアウト、接続リトライ
			WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
			tout = 0;
		}
	}
	Serial.println("WiFi Connected.");
	M5Dial.Display.fillScreen(BLACK);
	M5Dial.Display.drawString("Connected.", sx, sy);
	
	Serial.printf("sx=%d, sy=%d\n", sx, sy);
	
	redraw = 0;
	// Nikkeiタスク起動
	xTaskCreatePinnedToCore(NikkeiTask, "NikkeiTask", 8192, NULL, 1, NULL, 1);
	M5Dial.Display.fillScreen(BLACK);
	M5Dial.Display.drawString("Getting Nikkei...", sx, sy);
	
	// 日経平均取得待ち
	while(redraw == 0){
		delay(500);
	}
	Serial.print("\n");
	redraw = 0;
	
	M5Dial.Display.fillScreen(BLACK);
	// NIKKEI
	M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
	M5Dial.Display.setTextSize(0.8);
	M5Dial.Display.setTextColor(WHITE, BLACK);
	M5Dial.Display.drawString("NIKKEI", sx, sy-50);
	// 取得時間
	sprintf(buf, "%02d:%02d:%02d", bhour, bmin, bsec);
	M5Dial.Display.setTextFont(7);
	M5Dial.Display.setTextSize(0.5);
	M5Dial.Display.setTextColor(OLIVE, BLACK);
	M5Dial.Display.drawString(buf, sx, sy+50);
	// 円価値
	sprintf(buf, "%.2f", nowyen);
	M5Dial.Display.setTextFont(7);
	M5Dial.Display.setTextSize(0.65);
	M5Dial.Display.setTextColor(WHITE, BLACK);
	M5Dial.Display.drawString(buf, sx, sy);
	
}

// メインループ
void loop() {
	// put your main code here, to run repeatedly:
	M5Dial.update();
	if(M5Dial.BtnA.wasPressed()){
		bup = 0;
		bdown = 0;
		M5Dial.Display.fillArc(sx, sy, 100, 120, 0, 360, BLACK);
	}
	
	if(redraw != 0){
		// NIKKEI
		M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
		M5Dial.Display.setTextSize(0.8);
		M5Dial.Display.setTextColor(WHITE, BLACK);
		M5Dial.Display.drawString("NIKKEI", sx, sy-50);
		// 取得時間
		sprintf(buf, "%02d:%02d:%02d", bhour, bmin, bsec);
		M5Dial.Display.setTextFont(7);
		M5Dial.Display.setTextSize(0.5);
		M5Dial.Display.setTextColor(OLIVE, BLACK);
		M5Dial.Display.drawString(buf, sx, sy+50);
		// 円価値
		sprintf(buf, "%.2f", nowyen);
		M5Dial.Display.setTextFont(7);
		M5Dial.Display.setTextSize(0.65);
		if(redraw > 0){
			M5Dial.Display.setTextColor(RED, BLACK);
		}
		else if(redraw < 0){
			M5Dial.Display.setTextColor(GREEN, BLACK);
		}
		else{
			M5Dial.Display.setTextColor(WHITE, BLACK);
		}
		M5Dial.Display.drawString(buf, sx, sy);
		
		Serial.printf("NIKKEI = %.2f (%02d:%02d:%02d)\n", nowyen, bhour, bmin, bsec);
		
		if(redraw > 0){
			if(bup < 180){
				bup++;
				if(bup < 90){
					M5Dial.Display.fillArc(sx, sy, 100, 120, 270, 270+bup, RED);
					M5Dial.Display.fillArc(sx, sy, 100, 120, 270+bup, 360, BLACK);
					M5Dial.Display.fillArc(sx, sy, 100, 120, 0, 90, BLACK);
				}
				else{
					M5Dial.Display.fillArc(sx, sy, 100, 120, 270, 360, RED);
					M5Dial.Display.fillArc(sx, sy, 100, 120, 0, bup-90, RED);
					M5Dial.Display.fillArc(sx, sy, 100, 120, bup-90, 90, BLACK);
				}
			}
		}
		else{
			if(bdown < 180){
				bdown++;
				M5Dial.Display.fillArc(sx, sy, 100, 120, 90, 270-bdown, BLACK);
				M5Dial.Display.fillArc(sx, sy, 100, 120, 270-bdown, 270, GREEN);
			}
		}
		
		redraw = 0;
	}
	else{
		delay(20);
	}
}


// 日経平均チェック用タスク
void NikkeiTask(void* arg) 
{
	int httpCode;
	double rate;
	JsonObject obj;
	JsonObject result;
	char *str;
	char ss[10];
	// json用メモリ確保
	DynamicJsonDocument doc(1024);
	
	Serial.println("Start NikkeiTask.");
	
	while(1){
		if((WiFi.status() == WL_CONNECTED)){
			HTTPClient http;
			
			// HTTP接続開始
			http.begin(apiServer);
			
			// リクエスト作成
			httpCode = http.GET();
			
			// 返信
			if(httpCode > 0){
				// 応答取得
				String payload = http.getString();
				// ペイロードをjson変換
				deserializeJson(doc, payload);
				obj = doc.as<JsonObject>();
				
				result = obj[String("chart")][String("result")][0][String("meta")];
				rate = result[String("regularMarketPrice")];
				oldyen = nowyen;
				nowyen = rate;
				
				// time（20分前の時間が入る）
				time_t tt = result[String("regularMarketTime")];
				tt += 32400;
				struct tm *tm = gmtime(&tt);
				bhour = tm->tm_hour;
				bmin = tm->tm_min;
				bsec = tm->tm_sec;
				
				Serial.printf("%.2f : %.2f : %d\n", nowyen, oldyen, tt);
				
				if(nowyen > oldyen){
					redraw = 1;
				}
				else if(nowyen < oldyen){
					redraw = -1;
				}
				if(redraw != 0){
					Serial.printf("Nikkei %.2f yen\n", nowyen);
					Serial.printf("%02d:%02d:%02d\n", bhour, bmin, bsec);
				}
			}
			else{
				Serial.print("x");
			}
			http.end();
		}
		else{
			Serial.print("WiFi Error!");
			// 再接続
			WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		}
		vTaskDelay(5000);
	}
	
	//Serial.println("STOP NikkeiTask.");
	vTaskDelay(10);
	// タスク削除
	vTaskDelete(NULL);
}
