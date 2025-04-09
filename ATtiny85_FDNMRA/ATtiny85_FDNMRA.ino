//--------------------------------------------------------------------------------
//
// ATtiny85 LED ファンクションデコーダスケッチ NMRA認証
// Copyright(C)'2025 Ayanosuke(Maison de DCC) / Desktop Station
// [ATtiny85_FDNMRA.ino]
//
// PIN_AUX1 1      // Atiny85 PB1(6pin)V6[pad3] analogwrite AUX1 light F1
// PIN_AUX2 4      // Atiny85 PB4(3pin)V5[pad2] analogwrite AUX2
// PIN_F0_F 0      // Atiny85 PB0(5pin)V8[pad7] analogwrite head light F0
// PIN_F0_R 3      // Atint85 PB3(2pin)V7[pad6]             tail light F0
//
// AYA0071
// PIN_F0_F 0      // Atiny85 PB0(5pin)O7 analogwrite head light F0
// PIN_F0_R 1      // Atiny85 PB1(6pin)O6 analogwrite tail light F1
// PIN_AUX2 3      // Atint85 PB3(2pin)O3             sign light F3
// PIN_AUX1 4      // Atiny85 PB4(3pin)O2 analogwrite room light F5

//
// http://maison-dcc.sblo.jp/ http://dcc.client.jp/ http://ayabu.blog.shinobi.jp/
// https://twitter.com/masashi_214
//
// DCC電子工作連合のメンバーです
// https://desktopstation.net/tmi/ https://desktopstation.net/bb/index.php
//
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php
//
// CV49=0 or 1
// CV50 速度の閾値
// CV51 停止時の明るさ
// CV52 走行時の明るさ
// F0 で　On/Off ヘッドランプ切り替え
// F1 で　On/Off テールランプ切り替え
//
// CV49=20
// CV50 速度の閾値
// CV51 停止時の明るさ
// CV52 走行時の明るさ
//
// CV49 = 2- 13
// FX効果
// F0 で　On/Off ヘッドランプは自動切り替え
//
// 2021/08/11 CV29 bit1 によるFrd／Rev処理追加
//--------------------------------------------------------------------------------

// O1:F3 でON/OFF head sign
// O2:F0 でON/OFF head light
// O3:F1 でON/OFF tail light
// O4:F5 でON/OFF room light

#include <arduino.h>
#include "DccCV.h"
#include "NmraDcc.h"
#include "SeqLight.h"
#include <avr/eeprom.h>	 //required by notifyCVRead() function if enabled below

bool doing_Resets_Flag = false;  // True if we are simulating reset packets.
#define RESETDBG false




//使用クラスの宣言
NmraDcc	 Dcc;
DCC_MSG	 Packet;


//Internal variables and other.
#if defined(DCC_ACK_PIN)
const int DccAckPin = DCC_ACK_PIN ;
#endif

struct CVPair {
  uint16_t	CV;
  uint8_t	Value;
};
CVPair FactoryDefaultCVs [] = {
  {CV_MULTIFUNCTION_PRIMARY_ADDRESS, DECODER_ADDRESS}, // CV01
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, 0},               // CV09 The LSB is set CV 1 in the libraries .h file, which is the regular address location, so by setting the MSB to 0 we tell the library to use the same address as the primary address. 0 DECODER_ADDRESS
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB, 0},          // CV17 XX in the XXYY address
  {CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB, 0},          // CV18 YY in the XXYY address
//  {CV_29_CONFIG, 128 },                                // CV29 Make sure this is 0 or else it will be random based on what is in the eeprom which could caue headaches
  {CV_29_CONFIG, 0 },                                // CV29 Make sure this is 0 or else it will be random based on what is in the eeprom which could caue headaches  
  {CV_49_F0_FORWARD_LIGHT, 20},                        // CV49 F0 Forward Light
  {CV_DIMMING_SPEED, 1},                               // CV50 明暗を切り返るスピードの閾値
  {CV_DIMMING_LIGHT_QUANTITY,10},                      // CV51 低速時のヘッドライトの明るさ 
  {CV_HEADLIGHT_DIMMING, 250},                         // CV52 走行時のヘッドライトの明るさ
  {CV_ROOM_DIMMING, 128},                              // CV53 室内灯の明るさ
  {CV_dummy,0},
};

void(* resetFunc) (void) = 0;  //declare reset function at address 0
void LightMes( char,char );
void pulse(void);
uint8_t FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs) / sizeof(CVPair);

void notifyDccReset(uint8_t hardReset );

void notifyCVResetFactoryDefault()
{
  //When anything is writen to CV8 reset to defaults.

  resetCVToDefault();
  //Serial.println("Resetting...");
  delay(1000);  //typical CV programming sends the same command multiple times - specially since we dont ACK. so ignore them by delaying

  resetFunc();
};


extern void notifyServiceMode(bool svc)
{
}


//------------------------------------------------------------------
// Resrt
//------------------------------------------------------------------
void notifyDccReset(uint8_t hardReset )
{
  digitalWrite(PIN_F0_F,LOW);
  digitalWrite(PIN_F0_R,LOW);
  digitalWrite(PIN_AUX1,LOW);
  digitalWrite(PIN_AUX2,LOW);

  gState_F0 = 0;
  gState_F1 = 0;
  gState_F2 = 0;
  gState_F3 = 0; 
  gState_F4 = 0;
  gState_F5 = 0;
}

//------------------------------------------------------------------
// CVをデフォルトにリセット(Initialize cv value)
// Serial.println("CVs being reset to factory defaults");
//------------------------------------------------------------------
void resetCVToDefault()
{
  for (int j = 0; j < FactoryDefaultCVIndex; j++ ) {
    Dcc.setCV( FactoryDefaultCVs[j].CV, FactoryDefaultCVs[j].Value);
  }
};



extern void	   notifyCVChange( uint16_t CV, uint8_t Value) {
  //CVが変更されたときのメッセージ
  //Serial.print("CV ");
  //Serial.print(CV);
  //Serial.print(" Changed to ");
  //Serial.println(Value, DEC);
};

//------------------------------------------------------------------
// CV Ack
//------------------------------------------------------------------
void notifyCVAck(void)
{
  //Serial.println("notifyCVAck");
  digitalWrite(PIN_F0_F,ON);
  digitalWrite(PIN_F0_R,ON);
  digitalWrite(PIN_AUX1,ON);
  digitalWrite(PIN_AUX2,ON);

  delay( 6 );

  digitalWrite(PIN_F0_F,OFF);
  digitalWrite(PIN_F0_R,OFF);
  digitalWrite(PIN_AUX1,OFF);
  digitalWrite(PIN_AUX2,OFF);
}

//------------------------------------------------------------------
// Arduino固有の関数 setup() :初期設定
//------------------------------------------------------------------
void setup()
{
//TCCR1 = 0<<CTC1 | 0<<PWM1A | 0<<COM1A0 | 1<<CS10;
  
  pinMode(PIN_F0_F, OUTPUT);
  digitalWrite(PIN_F0_F, OFF);
  pinMode(PIN_F0_R, OUTPUT);
  digitalWrite(PIN_F0_R, OFF);
  pinMode(PIN_AUX1, OUTPUT);
  digitalWrite(PIN_AUX1, OFF);
  pinMode(PIN_AUX2, OUTPUT);
  digitalWrite(PIN_AUX2, OFF);

  Dccinit();

  //Reset task
  gPreviousL5 = millis();
}

//---------------------------------------------------------------------
// Arduino main loop
//---------------------------------------------------------------------
void loop() {
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  if ( (millis() - gPreviousL5) >= 10) // 100:100msec  10:10msec  Function decoder は 10msecにしてみる。
  {
    //Headlight control
    HeadLight_Control();

    //Motor drive control
    //Motor_Control();

    //Reset task
    gPreviousL5 = millis();
  }
}


//---------------------------------------------------------------------
// HeadLight control Task (10Hz:100ms)
//---------------------------------------------------------------------
void HeadLight_Control()
{
  uint16_t aPwmRef = 0;
  uint8_t lDir = 0;

  if(gCV29Direction == 1){            //REVをFWDにする  
    if( gDirection == 0 ){
      lDir = 1;
    } else {
      lDir = 0;
    }
  } else {
    lDir = gDirection;
  }
  
  //---------------------------------------------------------------------
  // ヘッド・テールライトコントロール
  // CV49 = 2 〜 13 設定で有効
  // F0 で　On/Off
  //--------------------------------------------------------------------- 
  if((gCV49_fx >= 2) && (gCV49_fx <= 13)) {
    FXeffect_Control();
  }

  //---------------------------------------------------------------------
  // ヘッド・テールライトコントロール
  // CV49 = 20 設定で有効
  // F0 で　On/Off
  //--------------------------------------------------------------------- 
  if(gCV49_fx == 20) { // 14
    if(gState_F0 == 0){                 // DCC F0=OFF コマンドの処理
      analogWrite(PIN_F0_F,0);
      digitalWrite(PIN_F0_R, OFF);      // 消灯
    } else {                            // DCC F0=ON コマンドの処理
      if( lDir == 0){             // Reverse 後進(DCS50Kで確認)
        digitalWrite(PIN_F0_R, ON);   // 点灯
        analogWrite(PIN_F0_F,0);          // O2:analog out (hedd light)   
      } else {                          // Forward 前進(DCS50Kで確認)
        digitalWrite(PIN_F0_R, OFF);
        if ( gSpeedRef >= gCV50_DimmingSpeed ){   // 調光機能 青箱はCV50=2以上にする事。1では反応しない
          aPwmRef = gCV52_HeadLighDimming;        // 速度が1以上だったらMAX
        } else {
          aPwmRef = gCV51_DimmingLightQuantity;    // 減光レベル
        }
        analogWrite(PIN_F0_F,aPwmRef);
      }
    }
  }

  //---------------------------------------------------------------------
  // ヘッド・テールライトコントロール
  // CV49=0 or 1 設定で有効
  // F0 で　Head Light On/Off F1 Tail Light On/Off F3 HEAD sign F5 room 
  //--------------------------------------------------------------------- 
  if((gCV49_fx == 0) || (gCV49_fx == 1)) {
    if(gState_F0 == 0){
      analogWrite(PIN_F0_F,0);
    } else {
      if ( gSpeedRef >= gCV50_DimmingSpeed ){   // 調光機能
        aPwmRef = gCV52_HeadLighDimming;        // 速度が1以上だったらMAX
      } else {
        aPwmRef = gCV51_DimmingLightQuantity;    // 減光レベル
      }
      analogWrite(PIN_F0_F,aPwmRef);
    }

    if(gState_F1 == 0){
      digitalWrite(PIN_F0_R, OFF);
    } else {
      digitalWrite(PIN_F0_R, ON);
    }
  }

  //---------------------------------------------------------------------
  // ヘッド・テールライトコントロール
  // F3 と F5の処理
  //--------------------------------------------------------------------- 

  // F2 受信時の処理
  // DCS50KのF2は1shotしか光らないので、コメントアウト
#if 0 
  if(gCV49_fx < 20 ) {  // CV49 が20 または21 以外有効
    if(gState_F2 == 0){                   
      digitalWrite(O1, OFF);  
    } else {
      digitalWrite(O2, ON);
    }
  }
#endif

// F3 受信時の処理
//#if 0
  if(gState_F3 == 0){
    digitalWrite(PIN_AUX1, OFF);
  } else {
    digitalWrite(PIN_AUX1, ON);
  }
//#endif

// F4 受信時の処理
#if 0
  if(gState_F4 == 0){
    digitalWrite(PIN_AUX1, OFF);
  } else {
    digitalWrite(PIN_AUX1, ON);
  }
#endif

// F5 受信時の処理
  if(gState_F5 == 0){
    analogWrite(PIN_AUX2, 0);
  } else {
    analogWrite(PIN_AUX2,128);//gCV53_RoomDimming);
  }
}



//DCC速度信号の受信によるイベント
//extern void notifyDccSpeed( uint16_t Addr, uint8_t Speed, uint8_t ForwardDir, uint8_t MaxSpeed )
extern void notifyDccSpeed( uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps )
{

#if RESETDBG

  if (Speed > 64) {
//  if(doing_Resets_Flag == false) {
      doing_Resets_Flag = true;
      notifyDccReset(0);
//    }
  }
    else {
          doing_Resets_Flag = false;
    }

#endif

  
//  if ( gDirection != ForwardDir) // old NMRA
  if ( gDirection != Dir )
  {
//  gDirection = ForwardDir; // old NMRA
    gDirection = Dir;
  }
  gSpeedRef = Speed;
}




//---------------------------------------------------------------------
// FX効果ステートマシン
// 10ms周期で起動
// unsigned chart ptn[4][5]{{'I',0,0,1},{'S',20,255,1},{'S',40,0,1},{'E',0,0,1}};
//---------------------------------------------------------------------
void FXeffect_Control(){
  enum{
    ST_IDLE = 0,
    ST_CMDPRO,
    ST_TIMPRO,
    ST_STAY,
  };

  static char state = ST_IDLE;    // ステート
  static unsigned char adr = 0;      // アドレス
  static int timeUp = 0;    // 時間
  static float delt_v = 0;  // 100msあたりの増加量 
  static float pwmRef =0;


  if(gState_F0 == 0){ // F0 OFF
    state = 0; 
    adr = 0;
    timeUp = 0;
    pwmRef = 0;
    digitalWrite(PIN_F0_F, 0);
  }

  S00:  
  switch(state){
    case ST_IDLE:     // IDLE
      if(gState_F0 > 0){ // F0 ON
        adr = 0;
        timeUp = 0;
        pwmRef = 0;
        analogWrite(PIN_F0_F, 0);
        state = ST_CMDPRO;
        goto S00;     // 100ms待たずに再度ステートマシーンに掛ける
      }
      break;
      
    case ST_CMDPRO:       // コマンド処理
        if( ptn[adr][0]=='I'){ // I:初期化
          timeUp = ptn[adr][1];
          pwmRef = ptn[adr][2];
          delt_v = 0; // 変化量0
          analogWrite(PIN_F0_F, (unsigned char)pwmRef);
          adr++;
          state = ST_CMDPRO;
          goto S00;   // 100ms待たずに再度ステートマシーンに掛ける
        } else if( ptn[adr][0]=='E'){ // E:end
          state = ST_STAY;
        } else if( ptn[adr][0]=='L' ){  // L:Loop
          adr = 0;
          state = ST_CMDPRO;
          goto S00;   // 100ms待たずに再度ステートマシーンに掛ける
        } else if( ptn[adr][0]=='O' ){ // O:出力
          timeUp = ptn[adr][1];
          pwmRef = ptn[adr][2];
          delt_v = 0;
          state = ST_TIMPRO;          
        } else if( ptn[adr][0]=='S' ){ // S:sweep
          timeUp = ptn[adr][1];
          delt_v = (ptn[adr][2]-pwmRef)/timeUp;  // 変化量を算出
          state = ST_TIMPRO;
        }
      break;
      
    case ST_TIMPRO: // S02:時間カウント
      timeUp--;
      pwmRef = pwmRef + delt_v;
      if(pwmRef<=0){            // 下限、上限リミッタ
          pwmRef = 0;
      } else if(pwmRef>=255){
          pwmRef = 255;
      }
      analogWrite(PIN_F0_F, (unsigned char)pwmRef);      
      if( timeUp <= 0 ){
        adr ++;
        state = ST_CMDPRO;  //次のコマンドへ
      }
      break;

      case ST_STAY: // stay
      break;

      default:
      break;
  }
}

//---------------------------------------------------------------------------
//ファンクション信号受信のイベント
//FN_0_4とFN_5_8は常時イベント発生（DCS50KはF8まで）
//FN_9_12以降はFUNCTIONボタンが押されたときにイベント発生
//前値と比較して変化あったら処理するような作り。
//---------------------------------------------------------------------------
extern void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState)
{
  if( FuncGrp == FN_0_4) // F0〜F4の解析
  {
    if( gState_F0 != (FuncState & FN_BIT_00))
    {
      //Get Function 0 (FL) state
      gState_F0 = (FuncState & FN_BIT_00);
    }
    if( gState_F1 != (FuncState & FN_BIT_01))
    {
      //Get Function 1 state
      gState_F1 = (FuncState & FN_BIT_01);
    }
    if( gState_F2 != (FuncState & FN_BIT_02))
    {
      gState_F2 = (FuncState & FN_BIT_02);
    }
    if( gState_F3 != (FuncState & FN_BIT_03))
    {
      gState_F3 = (FuncState & FN_BIT_03);
    }
    if( gState_F4 != (FuncState & FN_BIT_04))
    {
      gState_F4 = (FuncState & FN_BIT_04);
    }
  }

  if( FuncGrp == FN_5_8)  // F5〜F8の解析
  {
    if( gState_F5 != (FuncState & FN_BIT_05))
    {
      //Get Function 0 (FL) state
      gState_F5 = (FuncState & FN_BIT_05);
    }
    if( gState_F6 != (FuncState & FN_BIT_06))
    {
      //Get Function 1 state
      gState_F6 = (FuncState & FN_BIT_06);
    }
    if( gState_F7 != (FuncState & FN_BIT_07))
    {
      gState_F7 = (FuncState & FN_BIT_07);
    }
    if( gState_F8 != (FuncState & FN_BIT_08))
    {
      gState_F8 = (FuncState & FN_BIT_08);
    }
  }
}
//------------------------------------------------------------------
// DCC初期化処理）
//------------------------------------------------------------------
void Dccinit(void)
{

  //DCCの応答用負荷ピン
#if defined(DCCACKPIN)
  //Setup ACK Pin
  pinMode(DccAckPin, OUTPUT);
  digitalWrite(DccAckPin, 0);
#endif

#if !defined(DECODER_DONT_DEFAULT_CV_ON_POWERUP)
  if ( Dcc.getCV(CV_MULTIFUNCTION_PRIMARY_ADDRESS) == 0xFF ) {   //if eeprom has 0xFF then assume it needs to be programmed
    notifyCVResetFactoryDefault();
  } else {
    //Serial.println("CV Not Defaulting");
  }
#else
  //Serial.println("CV Defaulting Always On Powerup");
  notifyCVResetFactoryDefault();
#endif

  // Setup which External Interrupt, the Pin it's associated with that we're using, disable pullup.
  Dcc.pin(0, 2, 0); // Atiny85 7pin(PB2)をDCC_PULSE端子に設定

  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 100,   FLAGS_MY_ADDRESS_ONLY , 0 );

  //Init CVs
  gCV1_SAddr = Dcc.getCV( CV_MULTIFUNCTION_PRIMARY_ADDRESS ) ;
  gCVx_LAddr = (Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_MSB ) << 8) + Dcc.getCV( CV_MULTIFUNCTION_EXTENDED_ADDRESS_LSB );

  gCV49_fx = Dcc.getCV( CV_49_F0_FORWARD_LIGHT );
  switch(gCV49_fx){
    case 0: ptn = ptn1;
            break;
    case 1: ptn = ptn1;
            break;
    case 2: ptn = ptn2; // CV49 = 2 もやっと点灯
            break;
    case 3: ptn = ptn3;    
            break;
    case 4: ptn = ptn4;
            break;
    case 5: ptn = ptn5;
            break;
    case 6: ptn = ptn6;
            break;
    case 7: ptn = ptn7;
            break;
    case 8: ptn = ptn8;
            break;
    case 9: ptn = ptn9;
            break;
    case 10: ptn = ptn10;
            break;
    case 11: ptn = ptn11;
            break;
    case 12: ptn = ptn12;
            break;
    case 13: ptn = ptn13;
            break;
    default:
            break;
  }
  gCV50_DimmingSpeed  = Dcc.getCV( CV_DIMMING_SPEED );          // 減光スピード
  gCV51_DimmingLightQuantity = Dcc.getCV( CV_DIMMING_LIGHT_QUANTITY );  // 減光レベル
  gCV52_HeadLighDimming = Dcc.getCV( CV_ROOM_DIMMING );
  gCV53_RoomDimming = Dcc.getCV( CV_ROOM_DIMMING );
}
