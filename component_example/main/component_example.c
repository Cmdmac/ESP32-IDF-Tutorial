#include <stdio.h>
#include "Arduino.h"

void app_main(void)
{
    pinMode(GPIO_NUM_4, OUTPUT);
    digitalWrite(GPIO_NUM_4, HIGH);
}
