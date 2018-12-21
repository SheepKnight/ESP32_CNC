#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "FS.h"
#include "SD.h"

#define _cs   16   // 3 goes to TFT CS
#define _dc   22   // 4 goes to TFT DC
#define _mosi 23  // 5 goes to TFT MOSI
#define _sclk 18  // 6 goes to TFT SCK/CLK
#define _rst  17  // ESP RST to TFT RESET
#define _miso     // Not connected
Adafruit_ILI9341 tft = Adafruit_ILI9341(_cs, _dc, _rst);

int x_pos = 0.00;
int y_pos = 0.00;
int z_pos = 0.00;
int e_pos = 0.00;

int steps_per_moves = 50;

char * parameters[26]; // https://reprap.org/wiki/G-code#Fields
//WITHOUT G, M, T, N & *n

bool isMounted = false;

typedef char typeName[50];

String currentFile = "";

bool beginExplore = true;
void * lastExplorer[4];

typedef void (*Callbacks)(String);

Callbacks buf_func = NULL;
String buf_str = "";

TaskHandle_t Task2;

HardwareSerial MySerial(2);

void readFromSd_buf(String fileName){
  MySerial.println(fileName);
  buf_func = &readFromSd;
  buf_str = fileName;
}

void readFromSd(String fileName){
  File file = SD.open(fileName);
  if(!file){
    popUp("Failed to open file", 2, true, NULL, "", NULL, "");
    return;
  }
  String currentBuffer = "";
  bool isComment = false;
  while(file.available()){
    int input = file.read();
    if(input !=59){
      if(input != 13 && input != 59){
        if(input != 10 && isComment == false){
          currentBuffer = currentBuffer + char(input);
        }
      }else{
        isComment = false;
        firstParse(&currentBuffer);
      }
    }else{
      isComment = true;
    }
  }
  file.close();
}

void popUp(char message[], int textSize, bool shouldClean, Callbacks exitFunction, String param1, Callbacks exitFunction2, String param2){
  int firstInput = digitalRead(35);
  int firstInput_2 = digitalRead(34);
  int selected = 0;
  bool shouldProcess = true;
  tft.drawRect(20, 20, 280, 200, ILI9341_BLUE);
  tft.fillRect(21, 21, 278, 198, ILI9341_BLACK);
  tft.setCursor(30, 30);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(textSize);
  tft.println(message);
  while(shouldProcess == true){
    if(exitFunction2 == NULL){
      if(digitalRead(35) == HIGH ){
        if(firstInput == LOW){
          delay(50);
          Serial.println("Made a choice");
          shouldProcess = false;
        }
      }else{
      firstInput = LOW;
      }
    
    }
  }
  if(selected == 0){
    buf_func = exitFunction;
    buf_str = param1;
  }
  if(shouldClean == true){
    tft.fillScreen(ILI9341_BLACK);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(35, INPUT);
  pinMode(34, INPUT);
  pinMode(15, OUTPUT);
  pinMode(4, OUTPUT);
  tft.begin();tft.setRotation(3);tft.fillScreen(ILI9341_BLACK); tft.setTextSize(3);tft.println("Starting...");delay(250);
  tft.fillScreen(ILI9341_BLACK);

  MySerial.begin(115200);

  xTaskCreatePinnedToCore(
                    dumpSerial,   /* Task function. */
                    "Task2",     /* name of task. */
                    30000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
  
  void (*responses[])(String) = {&mount, &explore, &nieh, &nieh};
  String names[] = {String("Mount"), String("Explore"), String("Print"), String("Settings")};
  String params[] = {String("Mount"), String("/"), String("Print"), String("Settings")};
  uint16_t colors[] = {ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE};
  drawMenu(4, names, responses, params, colors, NULL);

  
}

void exploreGoBack(String useless){
  int lastSlash = currentFile.lastIndexOf("/");
  if(lastSlash > 1){
    if(currentFile.endsWith("/")){
      currentFile.remove(lastSlash);
      lastSlash = currentFile.lastIndexOf("/");
      if(lastSlash>1){
        currentFile.remove(lastSlash);
      }else{
        currentFile.remove(lastSlash+1);
      }
    }else{
      currentFile.remove(lastSlash);
    }
    
  }else{
    currentFile = "/";
  }
  
  explore(currentFile);
}

void explore(String fileName){
  currentFile = fileName;
  if(beginExplore != true){
    free(lastExplorer[0]);
    free(lastExplorer[1]);
    free(lastExplorer[2]);
    free(lastExplorer[3]);
  }else{
    beginExplore = false;
  }
  char bufferIn[150];
  fileName.toCharArray(bufferIn, 150);
  //Serial.println(150);
  //Serial.println(bufferIn);
  const char * dirname = bufferIn;
  //Serial.println(dirname);
  
  short * types = (short *)malloc(50*sizeof(short));
  typeName * names = (typeName *)malloc(50*sizeof(typeName));
  uint16_t * colors = (uint16_t *)malloc(50*sizeof(uint16_t));
  Callbacks * inputResponses = (Callbacks *)malloc(sizeof(Callbacks) *50);
  //Serial.println("Created malloc.");
  lastExplorer[0] = types;
  lastExplorer[1] = names;
  lastExplorer[2] = colors;
  lastExplorer[3] = inputResponses;
  int qtty = listDir(dirname, types, names);
  //Serial.println("Has listed.");
  String strNames[qtty] = {};
  String strParams[qtty] = {};
  if(qtty >0){
    for(int i = 0; i < qtty; i++){

      switch(*(types+i)){
        case 2:
          * (colors + i) = ILI9341_GREEN;
          * (inputResponses + i) = &readFromSd_buf;
          if(currentFile.endsWith("/")){
            strParams[i] = String(currentFile) + String(* (names+i));
          }else{
            strParams[i] = String(currentFile) + "/" + String(* (names+i));
          }
          break;
        case 3 :
           * (colors + i) = ILI9341_RED;
           * (inputResponses + i) = NULL;
           strParams[i] = "";
          break;
        case 1 :
          * (colors + i) = ILI9341_BLUE;
          * (inputResponses + i) = &explore;
          if(currentFile != "/"){
            strParams[i] = String(currentFile) + "/" + String(* (names+i));
          }else{
            strParams[i] = String(currentFile) + String(* (names+i));
          }
          break;
      }
    }
    
    
    for(int i = 0; i < qtty; i++){
      strNames[i] = String(* (names+i));
      
    }
    drawMenu(qtty, strNames, inputResponses, strParams, colors, &exploreGoBack);
  }
  currentFile = currentFile + '/';
}

void nieh(String entree){
  MySerial.println("NIEHING");
  MySerial.println(entree);
}



void mount(String useless){
  //SPIClass SPISD;
  //SPISD.begin(18, 19, 23, 2);
    if(isMounted == false){
    if(!SD.begin(/*2, SPISD*/)){
      MySerial.println("Card Mount Failed");
      popUp("Card Mount Failed.", 2, true, NULL, "", NULL, "");
      return;
    }
    uint8_t cardType = SD.cardType();
  
    if(cardType == CARD_NONE){
      popUp("NO SD.", 2, true, NULL, "", NULL, "");
      return;
    }
    isMounted = true;
    popUp("Mount successful.", 2, true, NULL, "", NULL, "");
  }else{
    popUp("Already Mounted.", 2, true, NULL, "", NULL, "");
  }
}

int listDir(const char * dirname, short * types, typeName * names){
    File root = SD.open(dirname);
    if(!root){
        //Serial.println("Failed to open directory");
        return 0;
    }
    if(!root.isDirectory()){
        //Serial.println("Not a directory");
        return 0;
    }
    //Serial.println("OK FOR FILE CREATION.");
    File file = root.openNextFile();
    int ammountOfFiles=0;   
    while(file && ammountOfFiles < 49){
        if(file.isDirectory()){
            //Serial.println("Type;");
            * (types+ammountOfFiles)= 1;
            //Serial.println("Name;");
            memset(names[ammountOfFiles],'\0',50);
            int len = currentFile.length();
            if(len>1){
              len = len+1;
            }
            memset( names[ammountOfFiles],'/',1);
            memcpy( names[ammountOfFiles], file.name() + len, strlen(file.name()));
            //memcpy( names[ammountOfFiles]/*- currentFile.length()-1*/, file.name() + len, strlen(file.name()));
            //Serial.println(currentFile);
            //memcpy(names+ammountOfFiles, file.name(), strlen(file.name())+1 );
            //Serial.println(file.name());
            ammountOfFiles++;
        } else {
            String fileName = String(file.name());
            
            if(fileName.endsWith(".g") || fileName.endsWith(".gco") || fileName.endsWith(".gcode") ){
              
              * (types+ammountOfFiles)= 2;
            }else{
              
              * (types+ammountOfFiles)= 3;
            }
            
            memset (names[ammountOfFiles],'\0',50);
            int len = currentFile.length();
            if(len>1){
              len = len+1;
            }
            memcpy( names[ammountOfFiles]/*- currentFile.length()-1*/, file.name() + len, strlen(file.name()));
            //Serial.println(names[ammountOfFiles]);
            //Serial.println("**************");
            //tft.print("  SIZE: ");
            //tft.println(file.size());
            ammountOfFiles++;
            //Serial.println("gud;");
        }
        file = root.openNextFile();
    }
    return(ammountOfFiles);
}

void drawMenu(int quantity, String names[], void (*inputResponses[])(String), String parameters[], uint16_t colors[], void(*secFunc)(String)){
  
  bool shouldProcess = true;
  bool hasBeenNeutral = true;
  long millisHeld = 0;
  short selected = 0;
  short lastSelected = 1;
  int firstInput = digitalRead(35);
  int firstInput_2 = digitalRead(34);
  bool shouldSec = false;
  while(shouldProcess == true){
    int joyY = analogRead(13);
    if(joyY > 3072){
      if(hasBeenNeutral){
        hasBeenNeutral = false;
        millisHeld = millis();
        selected++;
        if(selected > quantity - 1){
            selected = 0;
        }
      }else{
        if(millisHeld + 850 < millis()){
          millisHeld = millis() - 150;
          selected++;
          if(selected > quantity - 1){
            selected = 0;
          }
        }
      }
    }
    else if(joyY < 256){
      if(hasBeenNeutral){
        hasBeenNeutral = false;
        millisHeld = millis();
        selected--;
        if(selected < 0){
            selected = quantity - 1;
        }
      }else{
        if(millisHeld + 850 < millis()){
          millisHeld = millis() - 150;
          selected--;
          if(selected < 0){
            selected = quantity - 1;
          }
        }
      }
    }else{
      if(millisHeld + 200 < millis()){
        hasBeenNeutral = true;
        millisHeld = 0;
      }
    }
    if(digitalRead(35) == HIGH && inputResponses[selected] != NULL){
      if(firstInput == LOW){
        delay(50);
        shouldProcess = false;
      }
    }else{
      firstInput = LOW;
    }
    if(digitalRead(34) == HIGH && secFunc != NULL){
      if(firstInput_2 == LOW){
        delay(50);
        shouldProcess = false;
        shouldSec = true;
      }
    }else{
      firstInput_2 = LOW;
    }
    if(selected != lastSelected){
      lastSelected = selected;
      tft.fillScreen(ILI9341_BLACK);
      tft.setCursor(0, 0);
      tft.setTextColor(ILI9341_WHITE);
      tft.setTextSize(3);
      tft.println("Select :");
      tft.setTextSize(2);
      for(int i = 0; i < quantity; i++){
        if(selected == i){
          tft.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
        }else{
          tft.setTextColor(colors[i]);
        }
        tft.println(names[i]);
      }
    }
  }
  if(shouldSec == false){
    
    //inputResponses[selected](parameters[selected]);
    buf_func = inputResponses[selected];
    buf_str = parameters[selected];
  }else{
    buf_str = "";
    buf_func = secFunc;
  }
  
}

void loop(){
  // put your main code here, to run repeatedly:

  if(buf_func == NULL){

    void (*responses[])(String) = {&mount, &explore, &nieh, &nieh};
    String names[] = {String("Mount"), String("Explore"), String("Print"), String("Settings")};
    String params[] = {String("Mount"), String("/"), String("Print"), String("Settings")};
    uint16_t colors[] = {ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE};
    drawMenu(4, names, responses, params, colors, NULL);
    
  }else{

    Callbacks temp = buf_func;
    String temp_str = buf_str;
    buf_func = NULL;
    buf_str = "";
    temp(temp_str);
  }

}

String currentBuffer = "";



int moveBuffer[4] = {0,0,0,0};
bool isMoving = false;

void moveHead(int x, int y, int z, int e){
  while(isMoving != false){}
  MySerial.printf("\nMoving : x = %d; y = %d; z =%d; e = %d;\n",x,y,z,e);
  moveBuffer[0] = x;
  moveBuffer[1] = y;
  moveBuffer[2] = z;
  moveBuffer[3] = e;
  isMoving = true;
}

void dumpSerial(void * pvParameters){
  while(true){
    if(isMoving == true){
      while(moveBuffer[0] != 0 || moveBuffer[1] != 0 || moveBuffer[2] != 0 || moveBuffer[3] != 0 ){
        MySerial.println(moveBuffer[0]);
      
        if(moveBuffer[0] > 0){
          digitalWrite(4, HIGH);
          delayMicroseconds(1);
          digitalWrite(15, HIGH);
          delayMicroseconds(300);
          digitalWrite(15, LOW);
          delayMicroseconds(300);
          moveBuffer[0]--;
        }else{
          digitalWrite(4, LOW);
          delayMicroseconds(1);
          if(moveBuffer[0] != 0){
            digitalWrite(15, HIGH);
            delayMicroseconds(300);
            digitalWrite(15, LOW);
            delayMicroseconds(300);
            moveBuffer[0]++;
          }
        }
        
      }
      isMoving = false;
    }
    bool isComment = false;
    while(Serial.available()){
      int input = Serial.read();
      if(input != 59){
        if(input != 13 && input != 59){
          if(input != 10 && isComment == false){
            currentBuffer = currentBuffer + char(input);
          }
        }else{
          isComment = false;
          firstParse(&currentBuffer);
        }
      }else{
        isComment = true;
      }
    }
  }
}

void firstParse(String * input){
  String currentBuffer = * input;
  int cs = 0;
  for(int i = 0; currentBuffer.charAt(i) != '*' && currentBuffer.charAt(i) != NULL; i++){
    cs = cs ^ currentBuffer.charAt(i);
  }
  cs &= 0xff;  // Defensive programming...
  String temp = currentBuffer;
  int star_pos = currentBuffer.indexOf("*");
  int readCS;
  if(star_pos != -1){
    readCS = temp.substring(star_pos+1).toInt();
    currentBuffer.remove(star_pos);
  }else{
    readCS = cs;
  }
  if(readCS == cs){
          //PARSING
    int parameters_qtty = parseParams(currentBuffer);
    char * command = getParam('M');
    if(command != NULL){
      int param = atoi(command);
      parseM(param); 
    }else{
      command = getParam('G');
      if(command != NULL){
        int param = atoi(command);
        parseG(param);
      }else{
        command = getParam('T');
        if(command != NULL){
          int param = atoi(command);
          MySerial.print("Should set to tool n ");
          MySerial.println(param);
        }else{
          MySerial.println("NO COMMAND ???");
        }
      }
    }
  }else{
    MySerial.println("ERROR PARSING.");
  }
  * input = "";
  clearParams();
}

void parseG(int command){
  MySerial.print("G : ");
  MySerial.println(command);
  switch(command){
    case 0:{
      float parameters[5] = {0,0,0,0,0};
      char names[5] = {'X', 'Y', 'Z', 'E', 'F'};
      int * pos[] = {&x_pos, &y_pos, &z_pos, &e_pos, NULL};
      int offset[5] = {0,0,0,0,0};
      for(int i = 0; i<5; i++){
        char* param = getParam(names[i]);
        if(param != NULL){
          parameters[i] = atof(param)*100;
          if(pos[i] != NULL){
            offset[i] = (int) parameters[i] - * pos[i] ;
          }
        }
      }
      MySerial.printf("Should Move [ X:%d, Y:%d, Z:%d, E:%d, F:%d]", offset[0], offset[1], offset[2], offset[3], offset[4]);

      moveHead(offset[0], offset[1], offset[2], offset[3]);
 
      x_pos = x_pos + offset[0];
      y_pos = y_pos + offset[1];
      z_pos = z_pos + offset[2];
      e_pos = e_pos + offset[3];
      MySerial.println("");
      
      Serial.println("ok");
    }
    break;
    case 1:{
      float parameters[5] = {0,0,0,0,0};
      char names[5] = {'X', 'Y', 'Z', 'E', 'F'};
      int * pos[] = {&x_pos, &y_pos, &z_pos, &e_pos, NULL};
      int offset[5] = {0,0,0,0,0};
      for(int i = 0; i<5; i++){
        char* param = getParam(names[i]);
        if(param != NULL){
          parameters[i] = atof(param)*100;
          if(pos[i] != NULL){
            offset[i] = (int) parameters[i] - * pos[i] ;
          }
        }
      }
      MySerial.printf("Should Move [ X:%d, Y:%d, Z:%d, E:%d, F:%d]", offset[0], offset[1], offset[2], offset[3], offset[4]);
      moveHead(offset[0], offset[1], offset[2], offset[3]);
      //PUT HERE CODE TO MOVE
      x_pos = x_pos + offset[0];
      y_pos = y_pos + offset[1];
      z_pos = z_pos + offset[2];
      e_pos = e_pos + offset[3];
      MySerial.println("");
      
      Serial.println("ok");
    }
    break;
    case 92:{
      int * pos[4] = {&x_pos, &y_pos, &z_pos, &e_pos};
      char names[5] = {'X', 'Y', 'Z', 'E'};
      for(int i =0; i < 4; i++){
        char* param = getParam(names[i]);
        if(param != NULL){
          * pos[i] =(int) atof(param)*100;
        }
      }
    }
    break;  
    default:{
      MySerial.print("Unknown : G");
      MySerial.println(command);
    }
    break;
  }
}

void parseM(int command){
  MySerial.print("M : ");
  MySerial.println(command);
  switch(command){
    case 20:{
      //LISTING THE SD https://reprap.org/wiki/G-code#M20:_List_SD_card
      short * types = (short *)malloc(50*sizeof(short));
      typeName * names = (typeName *)malloc(50*sizeof(typeName));
      int qtty;
      char* param = getParam('P');
      if(param == NULL){
        qtty = listDir("/",types, names);
      }else{
        const char * place = param;
        MySerial.println(place);
        qtty = listDir(place,types, names);
      }
      Serial.println("ok");
      for(int i = 0; i < qtty; i++){
        if(types[i] == 2){
          Serial.println(names[i]);
        }
      }
      free(types);
      free(names);
      }
      break;

    case 21:{
        //MOUNT SD CARD 
        if(isMounted == false){
          if(!SD.begin(/*2, SPISD*/)){
            MySerial.println("Card Mount Failed");
            return;
          }
          uint8_t cardType = SD.cardType();
        
          if(cardType == CARD_NONE){
            return;
          }
          isMounted = true;
          Serial.println("ok");
        }
      }
      break;
    case 105:{
      //SENDING THE TEMPERATURE https://reprap.org/wiki/G-code#M105:_Get_Extruder_Temperature
      Serial.println("ok T:50.0 B:25.0");
      }
      break;
    case 114:{
      //SENDING THE POSITION https://reprap.org/wiki/G-code#M114:_Get_Current_Position
      MySerial.printf("ok C: X:%.1f Y:%.1f Z:%.1f E:%.1f\n",(float) x_pos/100, (float) y_pos/100, (float)z_pos/100, (float)e_pos/100);
      Serial.printf("ok C: X:%.1f Y:%.1f Z:%.1f E:%.1f\n", (float) x_pos/100, (float) y_pos/100, (float)z_pos/100, (float)e_pos/100);
      }
      break;
    case 115:{
      //SEND FIRMWARE VERSION https://reprap.org/wiki/G-code#M115:_Get_Firmware_Version_and_Capabilities
      Serial.println("ok PROTOCOL_VERSION:0.1 FIRMWARE_NAME:Sheep_Print FIRMWARE_URL:http%3A//reprap.org MACHINE_TYPE:Mendel EXTRUDER_COUNT:1");
      }
      break;
    default:{
      MySerial.print("Unknown : M");
      MySerial.println(command);
      }
      break;
  }
}

int parseParams(String input){
  int params = 0;
  
  char key[26] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
  
  for(int i = 0; i < 26; i++){
    String buf_string = input;
    int char_pos = buf_string.indexOf(key[i]);
    if(char_pos != -1 && (char_pos < 2 || buf_string.charAt(char_pos-1) == ' ' )){
      char toFind = ' ';
      if(buf_string.charAt(char_pos+1)=='"'){
        toFind = '"';
        buf_string.remove(0, char_pos+2);
      }else{
        buf_string.remove(0, char_pos+1);
      }
      params++;
      
      int end_char_pos = buf_string.indexOf(toFind);
      if(end_char_pos != -1){
        buf_string.remove(end_char_pos);
        
        parameters[i] = (char *)malloc(buf_string.length()+1);
        char final_val[buf_string.length()+1];
        buf_string.toCharArray(final_val, buf_string.length()+1);
        memcpy(parameters[i], final_val, buf_string.length()+1);
      }else{
        parameters[i] = (char *)malloc(buf_string.length()+1);
        char final_val[buf_string.length()+1];
        buf_string.toCharArray(final_val, buf_string.length()+1);
        memcpy(parameters[i], final_val, buf_string.length()+1);
        
      }
    }else{
      parameters[i] = NULL;
    }
  }
  return(params);
}

void clearParams(){
  for(int i=0; i < 26; i++){
    if(parameters[i]!=NULL){
      free(parameters[i]);
    }
  }
}

char* getParam(char letter){
  String alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  return(parameters[alphabet.indexOf(letter)]);
}

