/************************************************
 * Display Tool für Fledermausdetektor
 *
 * Sampling Rate = 500ksps
 * Bins: Bin[HWORD/2] = 250kHz;
 * Umrechnung von bin Nr auf Frequenz: f(binNr) = 250 / number_of_Bins * binNr    [in kHz]
 ************************************************/

import processing.serial.*;

final String serialPort = "/dev/ttyACM0";
Serial myPort;  
int x,y;
int Number_of_Bins = 1024;
int count;

void setup() {
  size(1024,600);
  println("Serial Interfaces found!");
  println(Serial.list());
  myPort = new Serial(this, Serial.list()[0], 115200);
  x = 0;
  y = 0;
  count = 0;
  background(255);
  stroke(100);
  fill(100);
  line(0,height-30,width,height-30);
  for (int i=0; i<1024; i+=50) {
    line(i,0,i,height-20);
    text(i*250000/Number_of_Bins, i-20,height-5);
  }
  stroke(0,0,255);
}

void draw() {
  String myString;
  int lf = 10;    // Linefeed in ASCII
  boolean eod=false;
  int value;
  fill(100);
  rect(0,0,80,30);
  fill(255);
  text(250000/1024*mouseX+" Hz",10,25);
//  text(count,10,25);
  while ((myPort.available() > 0) && !eod) {
    myString = myPort.readStringUntil(lf);
    if (myString != null) {
      myString = myString.substring(0,myString.length()-2);   // remove CR+LF
      value = Integer.parseInt(myString);
      if (value == 32000) {
        eod = true;
        x=0;
        y=0;
        count++;
      }
      else {
        value = value * 5;
        println(value);
        if (value < 600) line(x,height-y-30,x+1,height-value-30);
//        if (value>50) text(x*250000/Number_of_Bins, x+1,height-value-10);
        x++;
        y=value;
      }
    }
  }
}
