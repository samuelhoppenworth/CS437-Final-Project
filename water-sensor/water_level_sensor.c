#include <wiringPi.h>
#include <stdio.h>
int main()
{
  wiringPiSetup();
  char val;
  {
    pinMode(1,INPUT);
  }
  
  while(1) { 
    val = digitalRead(1);
    if( val==1 ){
      printf("There are liquid!\n");
    } 
    else {
      printf("No liquid!\n");
    }	
  }
}
