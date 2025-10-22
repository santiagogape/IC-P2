void setup() {
  // put your setup code here, to run once:
  SerialUSB.begin(9600);
  while (!SerialUSB){;}
  SerialUSB.println("ready");
}

//blocking
void readLine(){
  if (SerialUSB.available() > 0 ){
    String message = SerialUSB.readStringUntil('\n');
    message.trim();
    SerialUSB.print("leido: "); SerialUSB.println(message);
  }
}



void loop() {
  // put your main code here, to run repeatedly:
  //readLine();
  readLine();
}
