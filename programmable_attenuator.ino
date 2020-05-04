
/*
AnritsuのMG3633Aから取り外したステップアッテネーター
329H10778 を制御するプログラム
WAVGATのチップはATMEGA328ではないのでドライバを指定しないと4MHzで動くらしい！

2020/05/03 7M4MON
*/

#define MAX_ATT 145 	// 60 + 20 + 20 + 20 + 10 + 8 + 4 + 2 + 1
uint8_t att_val, last_att_val;
bool effect_rt;

#include <TM1637Display.h>
#define CLK 11
#define DIO 10
TM1637Display display(CLK, DIO);

#define PCF8574A_ADDR 0x38
#include <Wire.h>

#define PIN_ENC_A 2      // interrupt pin
#define PIN_ENC_B 3      // rotary encoder 
#define PIN_ENC_P 8      // push switch of rotary encoder
#define PIN_DIGIT 9     // 10dB <-> 1dB
#define PIN_PRES1 4
#define PIN_PRES2 5
#define PIN_PRES3 6
#define PIN_LED_IMM 12
#define PIN_LED_DIFF 13
#define HOLD_TIME_MS (800)	// 長押しで即時反映モードとの切り替え
#define PIN_I2C_DT A4
#define PIN_I2C_CK A5
#define PIN_SER_TX 1
#define PIN_SER_RX 0

#define ATT_PRES1 0
#define ATT_PRES2 60
#define ATT_PRES3 MAX_ATT

#define SERIAL_CONTROL_ENABLE

void change_attenuator(){
	uint8_t v, b;
	bool s;

	att_val = ( att_val > MAX_ATT ) ? MAX_ATT : att_val;		// 範囲制限

	if (att_val == last_att_val ) return;		//変化なしの場合、なにもしない。

	v = att_val;
	b = 0;
	
	s = (v >= 60) ? true: false;
	b = s ? b | 0x80 : b;			// PIN_60DB
	v = s ? v-60 : v;
	
	s = (v >= 40) ? true: false;
	b = s ? b | 0x40 : b;			// PIN_40DB
	v = s ? v-40 : v;
	
	s = (v >= 20) ? true: false;
	b = s ? b | 0x20 : b;			// PIN_20DB
	v = s ? v-20 : v;
	
	s = (v >= 10) ? true: false;
	b = s ? b | 0x10 : b;			// PIN_10DB
	v = s ? v-10 : v;
	
	b |= v;							// PIN_8DB, PIN_4DB, PIN_2DB, PIN_1DB
    b = ~b;                         // LでATT enable、　Z = H = Thuruなので反転

	//ATTを物理的にセット
	Wire.beginTransmission(PCF8574A_ADDR);
  	Wire.write(b);
  	Wire.endTransmission();

	//表示を上書き
	display.showNumberDec(att_val * -1);
	#ifdef SERIAL_CONTROL_ENABLE
	Serial.write(att_val);				//シリアルで結果を通知
	#endif
 	last_att_val = att_val;
}

//ロータリーエンコーダーの割り込み
void read_rotary_enc(){
    bool dir;
	uint8_t diff;
    dir = (digitalRead(PIN_ENC_B) == digitalRead(PIN_ENC_A)) ? false : true;
	diff = digitalRead(PIN_DIGIT) ? 10 : 1;
	if (dir){		// MINUS 負の値になるのを防ぐ。
		att_val = (att_val > diff) ? (att_val - diff) : 0;
	}else{
		att_val += diff;
		att_val = att_val > MAX_ATT ? MAX_ATT : att_val;	//範囲制限
	}
	display.showNumberDec(att_val * -1);
}

void setup(){
    pinMode(PIN_ENC_A,  INPUT_PULLUP);
    pinMode(PIN_ENC_B,  INPUT_PULLUP);
    pinMode(PIN_ENC_P,  INPUT_PULLUP);
    pinMode(PIN_DIGIT,  INPUT_PULLUP);
    pinMode(PIN_PRES1,  INPUT_PULLUP);
    pinMode(PIN_PRES2,  INPUT_PULLUP);
    pinMode(PIN_PRES3,  INPUT_PULLUP);
    pinMode(PIN_SER_RX, INPUT_PULLUP);
    pinMode(PIN_SER_TX ,   OUTPUT);    
    pinMode(PIN_LED_IMM,   OUTPUT);
    pinMode(PIN_LED_DIFF,  OUTPUT);
    pinMode(PIN_I2C_CK,  INPUT_PULLUP);
    pinMode(PIN_I2C_DT,  INPUT_PULLUP);
	Wire.begin();
	Serial.begin(9600); 
	display.setBrightness(0x0f);
	last_att_val = 255;
	att_val = MAX_ATT;		//ToDo: eepromからロード
	effect_rt = false;		//ToDo: eepromからロード
	digitalWrite(PIN_LED_IMM, effect_rt);
    attachInterrupt(1, read_rotary_enc, CHANGE);   //1:Pin3 A エッジ両取り込み
	change_attenuator();
}

void loop(){
	#ifdef SERIAL_CONTROL_ENABLE
	 if (Serial.available() > 0){
		uint8_t cmd = Serial.read();
		if (cmd == 0xff){				//コマンドが0xffの場合は現在の設定値を返す
			Serial.write(last_att_val);
		}else{
     		att_val = cmd;
			change_attenuator();		//effect_rt がfalseでもSerialの場合は即実行する。
		 }		
    }
	#endif
  
	// 決定ボタンが押されたとき、ATTを物理的に変更
	// 長押しされた場合、即時反映モードを切り替える
    uint16_t hold_time;
	if (digitalRead(PIN_ENC_P) == LOW){
		delay(10);	//チャタリングの防止
		if (digitalRead(PIN_ENC_P) == LOW){
			change_attenuator();
			hold_time = 0;
			while(digitalRead(PIN_ENC_P) == LOW){
				delay(1);
				hold_time++;
				if (hold_time == HOLD_TIME_MS){
					effect_rt = effect_rt ? false : true;
					digitalWrite(PIN_LED_IMM, effect_rt);
				}else if(hold_time > HOLD_TIME_MS){
					hold_time = HOLD_TIME_MS + 1;	// オーバーフローの防止（65.535秒押し続けた場合に発生する）
				}
			}
			//ボタンが離された
			delay(10);	//チャタリングの防止
		}
	}

	// プリセットボタン処理 押されたら即反映する。
	// ToDo: プリセットボタン長押しで上書き
	if (digitalRead(PIN_PRES1) == LOW) {
		att_val = ATT_PRES1;
		change_attenuator();
	} else if (digitalRead(PIN_PRES2) == LOW) {
		att_val = ATT_PRES2;
		change_attenuator();
	} else if (digitalRead(PIN_PRES3) == LOW) {
		att_val = ATT_PRES3;
		change_attenuator();
	} 

	//差分がある場合はLEDで警告
	bool diff_alert = (last_att_val == att_val) ? false : true;
	digitalWrite(PIN_LED_DIFF, (uint8_t)diff_alert);
	// 即時反映モード時の変更処理
	//if (diff_alert && effect_rt){
	if (effect_rt){					//差分があるかどうかは change_attenuator でチェックする
		change_attenuator();
	}
}
