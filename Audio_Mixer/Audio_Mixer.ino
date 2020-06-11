

  /*  +---------------------------------------------------------+
   *  |  This project will control TDA7319 and PT2258 for       |
   *  |  controlling two input audio mixer + 1 mic input.       |
   *  |  BASS, MID, TREBLE control, fader control, CLOCK,       |
   *  |  sensitivity output limit setting.                      |
   *  |  Pin diagram                                            |
   *  |                                                         |
   *  |         LCD RST  2              A3   encoder button     |
   *  |         LCD CE   3              A2                      |
   *  |         LCD DC   4              A1   L audio input      |
   *  |         LCD Din  5              A0   R audio input      |
   *  |         LCD CLK  6              13                      |
   *  |                  7              12   encoder pin1       |
   *  |                  8              11   encoder pin2       |
   *  |       backlight  9              10                      |
   *  |                    A4 A5 A6 A7 ->analog button pin      |
   *  |              SDA<--|  |                                 |
   *  |              SCL<-----                                  |
   *  +---------------------------------------------------------*/

  // ********************* Libraries ************************
  #include <SPI.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_PCD8544.h>
  #include <Wire.h>
  #include <DS3231.h>
  #include <EEPROM.h>
  #include <ClickEncoder.h>
  #include <TimerOne.h>
  #include <TDA7319.h>
  #include <PT2258.h>

  //********************variables****************************
  #define btnCW      0
  #define btnCCW    1
  #define btnSELECT  2
  #define btnNONE    3
  #define WAVE_WIDTH 100

  // Encoder variables
  ClickEncoder *encoder;
  int16_t value,last;
  
  //Clock variables
  DS3231 Clock;
  //byte ADay, AHour, AMinute, ASecond, ABits;
  int year, month, date, DoW, hour, minute, second;
  float temperature;
  bool h12, PM, Century=false;

  // TONE variables
  TDA7319 audio;
  PT2258 mixer;
  
  // LCD variables
  Adafruit_PCD8544 lcd = Adafruit_PCD8544(6,5,4,3,2); //6,5,4,3,2 2,3,4,5,6
  #include "small5x7bold_font.h"
  char tmp[14];

  char *menu[] = {"Volume","Fader","Bass","Midrange","Treble","Mic #1","Mic #2","Settings"};
  char *monthofyear[] = {" ","Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "oct", "Nov", "Dec" };
  char *dayofweek[] = {" ","Sun","Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  char *clockchar[] = {"MINUTE","HOUR","DAY","DATE","MONTH","YEAR","EXIT"};
  char *settings[] = {"OUTPUT LIMIT","BACKLIGHT","DEVICE INFO","CLOCK","EXIT"};

  //***************** Other Variables *********************
  byte backlight;
  byte bass,mid,tre;
  float input_sens;
  int BmpWidth=2;
  int MaxBarWidth;
  
  //*****************Check Clock***************************
  void ReadDS3231(){
   second=Clock.getSecond();
   minute=Clock.getMinute();
   hour=Clock.getHour(h12, PM);
   DoW=Clock.getDoW();
   date=Clock.getDate();
   month=Clock.getMonth(Century);
   year=Clock.getYear();
   temperature=Clock.getTemperature();
   Wire.endTransmission();
  }
  
  void timerIsr() {
    encoder->service();
  }

  
  // ----------> BUTTON <------------
  int read_buttons() {
   int adc_key_in = analogRead(A7);
   
   if (adc_key_in > 1000) return btnNONE; 
   else if (adc_key_in < 10) return btnCW; 
   else if (adc_key_in < 450) return btnSELECT; 
   else if (adc_key_in < 650) return btnCCW;
   //else if (adc_key_in < 850) return btnSETUP;
   return btnNONE;
   
  }
  
  int dB(unsigned x){     // credit to https://www.youtube.com/channel/UCLE2SRkdTBHHXjxj7Rm6j8g
  int result;             // excellent arduino projects u can find in his channel
  unsigned level;
  
  result=0;
  level=1;
   while(true) {
    if(x<(level*18U)>>4) {
      return result;
    } else if(x<(level*20U)>>4) {
      return result+1;
    } else if(x<(level*23U)>>4) {
      return result+2;
    } else if(x<(level*25U)>>4) {
      return result+3;
    } else if(x<(level*29U)>>4) {
      return result+4;
    } else if(x<level*2) {
      return result+5;
    } else {
      result+=6;
      level<<=1;
    }
   } // while
  } // dB


  void setup() {
    // put your setup code here, to run once:
   Wire.begin();
   pinMode(9,OUTPUT);    // backlight pin
   pinMode(A7,INPUT);     // button input pin
   lcd.begin(0x40,1); // contrast, bias
   lcd.clearDisplay();
   lcd.setFont(Small5x7PLBold);

   backlight=EEPROM.read(0);
   analogWrite(9,backlight);
   encoder = new ClickEncoder(12, 11, A3);
   delay(300);
   audio.init();
   mixer.init();

   bass=EEPROM.read(3);
   mid=EEPROM.read(4);
   tre=EEPROM.read(5);
   //freemode=EEPROM.read(6);
   //volmax=EEPROM.read(7);
   //bassmax=EEPROM.read(8);
   input_sens=EEPROM.read(9);
   //peak_delay=(EEPROM.read(10)*10);
   audio.setBass(bass);
   audio.setMiddle(mid);
   audio.setTreble(tre);
   audio.setVolume(0);
   audio.setVolume(1);
   mixer.setChannelVolume(10,0);
   mixer.setChannelVolume(10,1);
   mixer.setChannelVolume(10,2);
   mixer.setChannelVolume(10,3);
   mixer.setChannelVolume(79,4);
   mixer.setChannelVolume(79,5);
   print_txt("PLEASE WAIT",CENTER,5,BLACK,1,1);
   print_txt("system init...",CENTER,15,BLACK,1,1);
   lcd.display();
   delay(1000);            // delay for TDA and PT device to initialized
   Timer1.initialize(800);
   Timer1.attachInterrupt(timerIsr);
   last=-1;
  // Initialize variables

   MaxBarWidth=82;
  }//setup

  void print_txt(char temp[14], int x, int y, int color, int textsizex, int textsizey){
    lcd.setTextSize(textsizex,textsizey);
    lcd.setTextColor(color);
    lcd.setCursor(x,y);
    lcd.printTxt(temp);
  }
  
  void loop() {
    // put your main code here, to run repeatedly:
    // define static variable
    static bool buttonWasReleased=true,adjustment=false,display_init=false;
    static int main_menu=0, selecteditem=0,mic1vol=0,mic2vol=0,selectitem=0;
    static byte lcd_menu=0,last_month=0,last_DoW=0,last_date=0,last_year=0;
    static byte last_min=0,last_sec=0,last_hour=0,timer=0,duty=0;
    static byte vol=1,last_vol=0;

    static int fader=0,line1=16,line2=16,clock_var=0;
    static float vum1=0,vum2=0,var=0;
    unsigned wave[2][WAVE_WIDTH];
    unsigned WaveMin[2];
    unsigned WaveMax[2];
    int BarWidth;
    static int PeakVal[2]={0,0};
    static int PeakCnt[2]={0,0};

    // Read real time clock DS3231
    ReadDS3231();

    // read encoder
   value += encoder->getValue();
   
    // Get waves
    for(int i=0; i<WAVE_WIDTH; i++) {    
      wave[0][i]=analogRead(A0);
      wave[1][i]=analogRead(A1);
    } // for i


   // Calculate WaveMin / WaveMax
    WaveMin[0]=WaveMin[1]=10000;
    WaveMax[0]=WaveMax[1]=0; 
    for(int i=0; i<2; i++) { 
      for(int j=0; j<WAVE_WIDTH; j++) {    
        if(wave[i][j]<WaveMin[i]) WaveMin[i]=wave[i][j];
        if(wave[i][j]>WaveMax[i]) WaveMax[i]=wave[i][j];
      } // for i
    } // for j


  
    // lcd and button sketch
    lcd_menu = read_buttons();
    if(value>last) lcd_menu=btnCW;
    else if(value<last) lcd_menu=btnCCW;
    switch(lcd_menu){
      case btnCW:
         if(buttonWasReleased){
          buttonWasReleased=false;
          switch(main_menu){
            case 0: // increasing volume maximum value = 47 steps
               if(!adjustment){
                adjustment=true;
                last_date=0;
               }
               if(vol<47) vol++;
               else vol=47;
               break;
            case 1:
               if(adjustment){
                switch(selecteditem){
                  case 1:
                      if(fader<10) fader++;
                      else fader=10;
                      break;
                  case 2:
                      if(bass<31) bass++;
                      else bass=31;
                      break;
                  case 3:
                      if(mid<31) mid++;
                      else mid=31;
                      break;
                  case 4:
                      if(tre<31) tre++;
                      else tre=31;
                      break;
                  case 5:
                      if(mic1vol<79) mic1vol++;
                      else mic1vol=79;
                      break;
                  case 6:
                      if(mic2vol<79) mic2vol++;
                      else mic2vol=79;
                      break;
                }
               }
               else{
                selecteditem++;
                selectitem++;
                if(selectitem>3) selectitem=3;
                if(selecteditem>7){
                  selecteditem=0;
                  selectitem=0;
                }
               }
               break;
            case 2:
               if(adjustment){
                switch(selecteditem){
                  case 0: // sensitivity
                      if(input_sens<80) input_sens++;
                      else input_sens=80;
                      break;
                  case 1:  // back light
                      if(backlight<255) backlight+=5;
                      else backlight=255;
                      break;
                }
               }
               else{
                selecteditem++;
                selectitem++;
                if(selectitem>3) selectitem=3;
                if(selecteditem>4){
                  selecteditem=0;
                  selectitem=0;
                }
               }
               break;
           /*case 3:  //------------->CLOCK<------------
               if(adjustment){
                switch(selecteditem){
                  case 0:
                     clock_var++;
                     if(clock_var>59) clock_var=0;
                     break;
                  case 1:
                     clock_var++;
                     if(clock_var>23) clock_var=0;
                     break;
                  case 2:
                     clock_var++;
                     if(clock_var>7) clock_var=1;
                     break;
                  case 3:
                     clock_var++;
                     if(month==4||month==6||month==9||month==11){
                      if(clock_var>30) clock_var=1;
                     }
                     else{
                      if(clock_var>31) clock_var=1;
                     }
                     break;
                  case 4:
                     clock_var++;
                     if(clock_var>12) clock_var=1;
                     break;
                  case 5:
                     if(clock_var<99) clock_var++;
                     else clock_var=99;
                     break;
                }
               }
               else{
                selecteditem++;
                selectitem++;
                if(selectitem>3) selectitem=3;
                if(selecteditem>6){
                  selecteditem=0;
                  selectitem=0;
                }
               }
               break;*/
          }
         }
         break;

      case btnCCW:
         if(buttonWasReleased){
          buttonWasReleased=false;
          switch(main_menu){
            case 0:  // decreasing volume minimum value = 0.
               if(!adjustment){
                adjustment=true;
                last_date=0;
               }
               if(vol>0) vol--;
               else vol=0;
               break;
            case 1:
               if(adjustment){
                switch(selecteditem){
                  case 1:
                      if(fader>-10) fader--;
                      else fader=-10;
                      break;
                  case 2:
                      if(bass>0) bass--;
                      else bass=0;
                      break;
                  case 3:
                      if(mid>0) mid--;
                      else mid=0;
                      break;
                  case 4:
                      if(tre>0) tre--;
                      else tre=0;
                      break;
                  case 5:
                      if(mic1vol>0) mic1vol--;
                      else mic1vol=0;
                      break;
                  case 6:
                      if(mic2vol>0) mic2vol--;
                      else mic2vol=0;
                      break;
                }
               }
               else{
                selecteditem--;
                selectitem--;
                if(selectitem<0) selectitem=0;
                if(selecteditem<0){
                  selecteditem=7;
                  selectitem=3;
                }
               }
               break;
            case 2:
               if(adjustment){
                switch(selecteditem){
                  case 0: // sensitivity
                      if(input_sens>60) input_sens--;
                      else input_sens=60;
                      break;
                  case 1:  // back light
                      if(backlight>0) backlight-=5;
                      else backlight=0;
                      break;
                }
               }
               else{
                selecteditem--;
                selectitem--;
                if(selectitem<0) selectitem=0;
                if(selecteditem<0){
                  selecteditem=4;
                  selectitem=3;
                }
               }
               break;
           /*case 3: // ---------->CLOCK<--------------
               if(adjustment){
                switch(selecteditem){
                  case 0:
                     clock_var--;
                     if(clock_var<0) clock_var=59;
                     break;
                  case 1:
                     clock_var--;
                     if(clock_var<0) clock_var=23;
                     break;
                  case 2:
                     clock_var--;
                     if(clock_var<1) clock_var=7;
                     break;
                  case 3:
                     clock_var--;
                     if(month==4||month==6||month==9||month==11){
                      if(clock_var<1) clock_var=30;
                     }
                     else{
                      if(clock_var<1) clock_var=31;
                     }
                     break;
                  case 4:
                     clock_var--;
                     if(clock_var<1) clock_var=12;
                     break;
                  case 5:
                     clock_var--;
                     if(clock_var<16) clock_var=16;
                     break;
                }
               }
               else{
                selecteditem--;
                selectitem--;
                if(selectitem<0) selectitem=0;
                if(selecteditem<0){
                  selecteditem=6;
                  selectitem=3;
                }
               }
               break;*/
          }
         } // BUTTONRELEASED
         break;

  // +---------------------------------------------------------------------------+
  
      case btnSELECT:
         if(buttonWasReleased){
          buttonWasReleased=false;
          switch(main_menu){
            case 0: // -------->go to fader page<----------
               main_menu=1;
               selecteditem=1;
               selectitem=1;
               adjustment=true;
               break;
            case 1:
               //*****************2nd level********************
               if(adjustment){
                adjustment=false;
               }
               else{
                switch(selecteditem){
                  case 0:
                      main_menu=0;
                      selecteditem=0;
                      last_year=0;
                      break;
                  case 1:
                  case 2:
                  case 3:
                  case 4:
                  case 5:
                  case 6:
                      adjustment=true;
                      break;
                  case 7:
                      main_menu=2;
                      selecteditem=0;
                      selectitem=0;
                      break;
                }
               }
               break;
            case 2:
                //*******************SETTINGS*****************
                if(adjustment) adjustment=false;
                else{
                  switch(selecteditem){
                    case 0: // output sensitivity
                    case 1: // backlight
                    case 2: // Device info
                        adjustment=true;
                        break;
                    case 3:  // clock
                        main_menu=0;
                        selecteditem=0;
                        selectitem=0;
                        last_year=0;
                        break;
                    case 4: // exit
                        main_menu=1;
                        selecteditem=7;
                        selectitem=2;
                        break;
                  }
                }
                break;
            /*case 3: // ------------->CLOCK<-------------
                if(adjustment){
                switch(selecteditem){
                  case 0:
                     Clock.setMinute(clock_var);
                     break;
                  case 1:
                     Clock.setHour(clock_var);
                     break;
                  case 2:
                     Clock.setDoW(clock_var);
                     break;
                  case 3:
                     Clock.setDate(clock_var);
                     break;
                  case 4:
                     Clock.setMonth(clock_var);
                     break;
                  case 5:
                     Clock.setYear(clock_var);
                     break;
                }
               adjustment=false;
               }
               else{
                if(selecteditem!=6){
                  switch(selecteditem){
                    case 0:
                        clock_var=minute;
                        break;
                    case 1:
                        clock_var=hour;
                        break;
                    case 2:
                        clock_var=DoW;
                        break;
                    case 3:
                        clock_var=date;
                        break;
                    case 4:
                        clock_var=month;
                        break;
                    case 5:
                        clock_var=year;
                        break;
                  }
                  adjustment=true;
                }
                else{
                  main_menu=2;
                  selecteditem=3;
                }
               }
               break;*/
          }
         }// buttonWasReleased
         break;

      case btnNONE:
          if(!buttonWasReleased){
            buttonWasReleased=true;
            timer=0;
            display_init=false;
            lcd.clearDisplay();
            audio.setVolume(vol*2);
            audio.setVolume((vol*2)+1);
            audio.setTreble(tre);
            audio.setMiddle(mid);
            audio.setBass(bass);
            last_date=0;
          }
          switch(main_menu){
            case 0:{

               if(last_month!=month||last_date!=date||last_DoW!=DoW||last_year!=year){
                snprintf(tmp,12,"%s%02d%s%02d", dayofweek[last_DoW],last_date,monthofyear[last_month],last_year);
                print_txt(tmp, CENTER, 0, WHITE, 1,1);
                snprintf(tmp,12,"%s%02d%s%02d", dayofweek[DoW],date,monthofyear[month],year);
                print_txt(tmp, CENTER, 0, BLACK, 1,1);
                last_year=year;
                last_month=month;
                last_DoW=DoW;
                last_date=date;
               }
               if(last_sec!=second){
                snprintf(tmp,14,"%02d:%02d:%02d", last_hour,last_min,last_sec);
                print_txt(tmp,CENTER,10,WHITE,1,1);
                snprintf(tmp,14,"%02d:%02d:%02d",hour,minute,second);
                print_txt(tmp,CENTER,10,BLACK,1,1);
                last_hour=hour;
                last_min=minute;
               }

               if(adjustment){
                if(!display_init){
                  snprintf(tmp,20,"VOLUME: %d dB ", vol);
                  print_txt(tmp,CENTER,28,BLACK,1,1);
                  lcd.drawRect(2,38,82,8,BLACK);
                  byte vol_bar=map(vol,0,47,0,80);
                  for(int i=0;i<vol_bar;i++){
                    lcd.drawLine((3+i),39,(3+i),44,BLACK);
                  }
                  for(int i=vol_bar;i<80;i++){
                    lcd.drawLine((3+i),39,(3+i),44,WHITE);
                  }
                  
                  display_init=true;
                }
               }
               else{
                if(!display_init){
                  display_init=true;
                  last_date=0;
                  lcd.clearDisplay();
                }
                else{
                    // Draw bar graphs
                      for(int i=0; i<2; i++) {
                        BarWidth=map((dB(unsigned(WaveMax[i]-WaveMin[i]))),0,60,0,MaxBarWidth);
                        if(BarWidth<0) BarWidth=0;
                        if(BarWidth>MaxBarWidth) BarWidth=MaxBarWidth;
    
                        if(BarWidth>=PeakVal[i]) {
                          PeakVal[i]=BarWidth;
                          PeakCnt[i]=0;
                        } // if

                        // fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
                        // drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
                        // drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)

                        // draw bar
                        if(BarWidth>0) lcd.fillRect(0,i*12+20,BarWidth,i*1+11,BLACK);
                        if(BarWidth<MaxBarWidth) lcd.fillRect(1+BarWidth,i*12+20,83,i*1+11,WHITE);
                        if(BarWidth>=input_sens){
                          if(vol>0){
                            vol--;
                            audio.setVolume(vol*2);
                            audio.setVolume((vol*2)+1);
                          }
                        }
    
                        // draw peak bar
                        if(PeakVal[i]>2) lcd.fillRect(PeakVal[i],i*12+20,2+PeakVal[i],i*1+11,BLACK);
                        if(PeakVal[i]<(MaxBarWidth-3)) lcd.fillRect(3+PeakVal[i],i*12+20,83,i*1+11,WHITE);

                        if(PeakVal[i]>0) {
                          PeakCnt[i]++;
                          if(PeakCnt[i]>=8) {
                            PeakVal[i]--;
                            PeakCnt[i]=0;
                          } // if
                        } // if
                      } // for i
                }
               }

            }
            break;

            case 1:
               if(!display_init){
                if(adjustment){
                  switch(selecteditem){
                    case 1:{
                       // fader
                       if(fader==0){  // fader at 0 (center)
                        line1=10;
                        line2=10;
                       }
                       else if(fader<0){   // fader (-10 to -1)
                        line1=10+fader;
                        line2=9-(fader*7);
                       }
                       else if(fader>0){  // fader from 1 to 10
                        line1=9+(fader*7);
                        line2=10-fader;
                       }
                       lcd.drawCircle(5, 40, 2, BLACK);
                       lcd.drawLine(9,40,75,40,BLACK);
                       lcd.drawLine(8,37,8,43,BLACK);
                       lcd.drawLine(24,38,24,42,BLACK);
                       lcd.drawLine(42,37,42,43,BLACK);
                       lcd.drawLine(60,38,60,42,BLACK);
                       lcd.drawLine(76,37,76,43,BLACK);
                       lcd.drawCircle(79,40,2,BLACK);
                       lcd.drawRect(41+(fader*3),38,3,5,BLACK);
                       byte bar1=map(fader,10,-10,0,35);
                       for(int i=0;i<bar1;i++){
                         lcd.drawLine(15,35-i,35,35-i,BLACK);
                       }
                       byte bar2=map(fader,-10,10,0,35);
                       for(int i=0;i<bar2;i++){
                         lcd.drawLine(49,35-i,69,35-i,BLACK);
                       }
                       mixer.setChannelVolume(line1,0);
                       mixer.setChannelVolume(line1,1);
                       mixer.setChannelVolume(line2,2);
                       mixer.setChannelVolume(line2,3);
                    }
                       break;
                    case 2:{  // ------------>BASS<---------------
                       snprintf(tmp,20,"BASS: %d dB ", bass);
                       print_txt(tmp,CENTER,15,BLACK,1,1);
                       lcd.drawRect(2,25,82,8,BLACK);
                       byte vol_bar=map(bass,0,31,0,80);
                       for(int i=0;i<vol_bar;i++){
                         lcd.drawLine((3+i),26,(3+i),32,BLACK);
                       }
                       EEPROM.write(3,bass);
                    }
                       break;
                    case 3:{  //  ----------->MID<--------------
                       snprintf(tmp,20,"MID: %d dB ", mid);
                       print_txt(tmp,CENTER,15,BLACK,1,1);
                       lcd.drawRect(2,25,82,8,BLACK);
                       byte vol_bar=map(mid,0,31,0,80);
                       for(int i=0;i<vol_bar;i++){
                         lcd.drawLine((3+i),26,(3+i),32,BLACK);
                       }
                       EEPROM.write(4,mid);
                    }
                       break;
                    case 4:{  // --------->TREBLE<---------------
                       snprintf(tmp,20,"TREBLE: %d dB ", tre);
                       print_txt(tmp,CENTER,15,BLACK,1,1);
                       lcd.drawRect(2,25,82,8,BLACK);
                       byte vol_bar=map(tre,0,31,0,80);
                       for(int i=0;i<vol_bar;i++){
                         lcd.drawLine((3+i),26,(3+i),32,BLACK);
                       }
                       EEPROM.write(5,tre);
                    }
                       break;
                    case 5:  // ---------->MIC #1 Volume<----------------
                       snprintf(tmp,20,"MIC1 VOL: %d dB ", mic1vol);
                       print_txt(tmp,CENTER,15,BLACK,1,1);
                       lcd.drawRect(1,25,81,8,BLACK);
                       for(int i=0;i<mic1vol;i++){
                        lcd.drawLine((2+i),26,(2+i),32,BLACK);
                       }
                       mixer.setChannelVolume(79-mic1vol,4);
                       break;
                    case 6:  // ---------->MIC #2 Volume<----------------
                       snprintf(tmp,20,"MIC2 VOL: %d dB ", mic2vol);
                       print_txt(tmp,CENTER,15,BLACK,1,1);
                       lcd.drawRect(1,25,81,8,BLACK);
                       for(int i=0;i<mic2vol;i++){
                        lcd.drawLine((2+i),26,(2+i),32,BLACK);
                       }
                       mixer.setChannelVolume(79-mic2vol,5);
                       break;
                  }
                }
                else{
                  print_txt("MAIN MENU",CENTER,0,BLACK,1,1);
                  for(int i=0;i<4;i++){
                    print_txt(menu[selecteditem-selectitem+i],6,10+(i*10),BLACK,1,1);
                  }
                  print_txt(">",0,10+(selectitem*10),BLACK,1,1);
                }
                display_init=true;
               }
               break;
            case 2:
               if(!display_init){
                if(adjustment){
                  switch(selecteditem){
                    case 0:{
                        print_txt("Sensitivity",CENTER,05,BLACK,1,1);
                        print_txt((dtostrf(input_sens,2,0,tmp)),CENTER,15,BLACK,1,1);
                        lcd.drawRect(2,25,82,8,BLACK);
                        byte vol_bar=map(input_sens,60,80,0,80);
                        for(int i=0;i<vol_bar;i++){
                          lcd.drawLine((3+i),26,(3+i),32,BLACK);
                        }
                        EEPROM.write(9,input_sens);
                    }
                        break;
                    case 1:
                        print_txt("Adjust backlyt",CENTER,5,BLACK,1,1);
                        lcd.drawRect(15,20,53,6,BLACK);
                        for(int i=0;i<(backlight/5);i++){
                         lcd.drawLine((16+i),21,(16+i),24,BLACK);
                        }
                        analogWrite(9,backlight);
                        EEPROM.write(0,backlight);
                        break;
                    case 2:  // Device info
                        print_txt("PRO-320W AMP",CENTER,0,BLACK,1,1);
                        print_txt("Mixer Amplifier",CENTER,10,BLACK,1,1);
                        print_txt("MicroControlled",CENTER,20,BLACK,1,1);
                        print_txt("Code: RV-301-01",CENTER,30,BLACK,1,1);
                        print_txt("By: Cris Villa",CENTER,40,BLACK,1,1);
                        break;
                  }
                }
                else{
                  print_txt("SETTINGS",CENTER,0,BLACK,1,1);
                  for(int i=0;i<4;i++){
                    print_txt(settings[selecteditem-selectitem+i],6,10+(i*10),BLACK,1,1);
                  }
                  print_txt(">",0,10+(selectitem*10),BLACK,1,1);
                }
                display_init=true;
               }
               break;
               
           /* case 3:  // Clock
                if(!display_init){
                 if(adjustment){
                  if(selecteditem==2){
                   print_txt(dayofweek[clock_var],CENTER,25,BLACK,1,1);
                  }
                  else if(selecteditem==4){
                   print_txt(monthofyear[clock_var],CENTER,25,BLACK,1,1);
                  }
                  else{
                    print_txt((dtostrf(clock_var,2,0,tmp)),CENTER,25,BLACK,1,1);
                  }
                 }
                 else{
                  print_txt("CLOCK",CENTER,0,BLACK,1,1);
                  for(int s=0;s<4;s++){
                    print_txt(clockchar[selecteditem-selectitem+s],6,10+(s*10),BLACK,1,1);
                  }
                  print_txt(">",0,10+(selectitem*10),BLACK,1,1);
                 }
                 display_init=true;
                }
                break;*/
               
        }
        if(main_menu!=0||selecteditem!=0||adjustment){
          if(timer<5){
            if(last_sec!=second) timer++;
          }
          else{
            main_menu=0;
            selecteditem=0;
            adjustment=false;
            display_init=false;
            timer=0;
            last_date=0;
            lcd.clearDisplay();
          }
        }
        break;
    }



    lcd.display();
    //**************save last values*******************
    last_sec=second;
    last=value;
  }
