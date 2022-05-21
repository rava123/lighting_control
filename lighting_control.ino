#include <MsTimer2.h>
#include <TimerOne.h>

#define DEBUG

const int ledPin = 13;         // LED接続ピン（定数）

const int entrance_pin = 2;
const int porch_pin = 4;
const int syokusai_tyushajou_pin = 6;
const int approach_pin = 8;

const int foto_pin = A4;
const int sb612_entrance_pin = A3;
const int sb612_porch_pin = A2;
const int sb612_north_pin = A1;
const int sb612_south_pin = A0;

const int NIGHT_THRESH = 2;
const int DAYLIGHT_THRESH = 70;
const int DETECT_THRESH = 100;
const int FOTO_CNT_MAX = 100;
const long NIGHT_LIGHT_TIME = 28800000; //ms
const long ENTRANCE_LIGHT_TIME = 20000; //ms
//const long NIGHT_LIGHT_TIME = 5000;
const long PORCH_LIGHT_TIME = 15000; //ms
const long APPROACH_LIGHT_TIME = 15000000; //us

int foto_value_avg;
long foto_value_sum = 0;
int foto_value_count_index;

/* 夜ならtrue */
bool night = false;

/* 夜ライトがついていたらtrue */
bool night_light = false;

/* 外センサーが反応したらtrue. */
bool human_detecting = false;

/* 玄関センサーが反応したらtrue */
bool entrance_detecting = false;

/* TimerOneの開始直後発火の回避フラグ*/
bool counting = false;

/* 玄関センサーを検知したとき、夜ライト時間のカウントを一旦とめて、玄関ライトをカウントする
　　一旦止めた時点でのカウントを保存し、玄関ライトを消したときに再開する。タイマーが足りないので苦肉の策。
*/
long remain_light_time;

long porch_up_edge_time;

void setOff()
{
  if (counting) {
    counting = false;
    return;
  }
  Timer1.stop();
  human_detecting  = false;
  digitalWrite(approach_pin, LOW);
}

void offNightLight()
{
  //照明を消す
  MsTimer2::stop();
  digitalWrite(syokusai_tyushajou_pin, LOW);
  digitalWrite(approach_pin, LOW);
  night_light = false;
}

void offEntranceLight()
{
  MsTimer2::stop();
  digitalWrite(entrance_pin, LOW);
  entrance_detecting = false;

  //夜ライトカウントを再開
  if (remain_light_time != 0) {
    MsTimer2::set(remain_light_time - ENTRANCE_LIGHT_TIME, offNightLight);
    MsTimer2::start();
  }
}

void setup()
{
  Serial.begin(9600);
  //pinMode(ledPin, OUTPUT);
  pinMode(entrance_pin, OUTPUT);
  pinMode(porch_pin, OUTPUT);
  pinMode(syokusai_tyushajou_pin, OUTPUT);
  pinMode(approach_pin, OUTPUT);
  
  pinMode(sb612_entrance_pin, INPUT);
  pinMode(sb612_porch_pin, INPUT);
  pinMode(sb612_north_pin, INPUT);
  pinMode(sb612_south_pin, INPUT);
  pinMode(foto_pin, INPUT);

  //初期値を消灯状態に
  digitalWrite(entrance_pin, LOW);
  digitalWrite(porch_pin, LOW);
  digitalWrite(syokusai_tyushajou_pin, LOW);
  digitalWrite(approach_pin, LOW);
  
  foto_value_avg = 10;
  foto_value_sum = 0;
  foto_value_count_index = 0;
}

void loop()
{
  int sb612_entrance_value = analogRead(sb612_entrance_pin);
  int sb612_porch_value = analogRead(sb612_porch_pin);
  int sb612_north_value = analogRead(sb612_north_pin);
  int sb612_south_value = analogRead(sb612_south_pin);
  
  int foto_value   = analogRead(foto_pin);
  if (foto_value_count_index < FOTO_CNT_MAX) {
    foto_value_count_index++;
    foto_value_sum += foto_value; 
  } else {
    foto_value_count_index = 0;
    foto_value_sum = foto_value;
  }

#ifdef DEBUG
  Serial.print("foto_value:");
  Serial.println(foto_value);
  Serial.print("foto_value_sum:");
  Serial.println(foto_value_sum);
#endif

  if (foto_value_count_index == FOTO_CNT_MAX) {
    int foto_value_avg = foto_value_sum / FOTO_CNT_MAX;
#ifdef DEBUG
    Serial.print("foto_value_avg: ");
    Serial.print(foto_value_avg);
    Serial.print(", night: ");
    Serial.print(night);
    Serial.print(", human_detecting: ");
    Serial.println(human_detecting);
#endif
    //夜モードでなく、人を検出してなければ夜モードにして夜照明をつける
    if (foto_value_avg <= NIGHT_THRESH && !night && !human_detecting) {
      digitalWrite(syokusai_tyushajou_pin, HIGH);
      digitalWrite(approach_pin, HIGH);
      MsTimer2::set(NIGHT_LIGHT_TIME, offNightLight);
      MsTimer2::start();
      night = true;
      night_light = true;
    }
    //夜モードで少し明るくなれば夜モードを終了する
    if (foto_value_avg >= DAYLIGHT_THRESH && night && !human_detecting) {
      night = false;
    }
  }

  //夜だけ点灯
  //玄関は常に点けてもよいかも
  if (sb612_entrance_value > DETECT_THRESH) {
    digitalWrite(entrance_pin, HIGH);
    //検出エッジが立ったとき、かつその時夜ライトカウントが動いているとき
    if (!entrance_detecting) {
      if (MsTimer2::count != 0) {
        //カウントが0じゃなければ、夜ライト点灯中.．
        //残り時間を保存しておく
        remain_light_time = MsTimer2::msecs - MsTimer2::count;
      } else {
        //カウントが0なら、夜ライトは消灯中
        remain_light_time = 0;
      }
    }
    entrance_detecting = true;
    MsTimer2::stop();
    MsTimer2::set(ENTRANCE_LIGHT_TIME, offEntranceLight);
    MsTimer2::start();
  }

  /* センサが反応したエッジの時間を保存 */
  if (sb612_porch_value > DETECT_THRESH && night) {
    porch_up_edge_time = millis();
  }
  
  if (abs(millis() - porch_up_edge_time) < PORCH_LIGHT_TIME) {
    digitalWrite(porch_pin, HIGH);    
  } else {
    digitalWrite(porch_pin, LOW);    
  }

  if ((sb612_south_value > DETECT_THRESH 
      || sb612_north_value > DETECT_THRESH 
      || sb612_porch_value > DETECT_THRESH) 
      && night && !night_light) {
    digitalWrite(approach_pin, HIGH);
    Timer1.initialize(APPROACH_LIGHT_TIME);
    Timer1.attachInterrupt(setOff);
    human_detecting = true;
    counting = true;
  }

  delay(300);
}

