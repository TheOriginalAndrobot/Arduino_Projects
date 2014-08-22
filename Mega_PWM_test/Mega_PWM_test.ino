int ledPin = 13;      // LED connected to digital pin 9
int pwmPin = 2;

int analogPin = 0;   // potentiometer connected to analog pin 3

int val = 0;         // variable to store the read value
int dir = 1;
int dtime = 0;



void setup()

{

  pinMode(ledPin, OUTPUT);   // sets the pin as output

}



void loop()
{

  //val = analogRead(analogPin);   // read the input pin
  

  //analogWrite(ledPin, val / 4);  // analogRead values go from 0 to 1023, analogWrite values from 0 to 255


  dtime = analogRead(analogPin);
  
  if(val==255 || val==0)
    dtime = 1000;


  analogWrite(pwmPin, val);
  
  digitalWrite(ledPin, dir);
  
  delay(dtime);
  
  if(dir)
    val++;
  else
    val--;
  
  if(val == 255)
    dir=0;
  else if(val == 0)
    dir=1;
  
}
