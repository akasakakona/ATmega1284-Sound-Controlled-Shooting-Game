/*	Author: Tangyuan Liang
 *  Partner(s) Name: 
 *	Lab Section:
 *	Assignment: Final Project Checkpoint 2
 *	Exercise Description: This is a volume-controlled game where the player has to avoid the character from getting touched by the enemies. 
 *  The player can move the character away from the enemy to temporarily dodge it, but eventually has to shoot the enemy by shooting a laser. 
 *  The laser is triggered by the user’s voice. The movement of the character is controlled by the joystick. The highest score is saved in the EEPROM 
 *  so that it will not get lost even when the power is cut off. It will still be there the next time it powers up.
 *  
 *  Demo Link: https://www.youtube.com/watch?v=Majb10uYn0U
 * 
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */

#ifndef F_CPU
#define F_CPU 8000000
#endif

#include <avr/io.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>

#include "timer.h"
#include "nokia5110.h"

typedef struct task {
    signed char state;
    unsigned long int period;
    unsigned long int elapsedTime;
    int (*TickFct)(int);
} task;

void ADC_init() {
    ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
    // ADEN: setting this bit enables analog-to-digital conversion.
    // ADSC: setting this bit starts the first conversion.
    // ADATE: setting this bit enables auto-triggering. Since we are
    //        in Free Running Mode, a new conversion will trigger whenever
    //        the previous conversion completes.
}

enum joystickStates {joystick_wait, joystick_left, joystick_right};
enum ADCStates {ADC_output};
enum LCDStates {LCD_start, LCD_game, LCD_end} LCDState;
enum enemyStates {enemy_move};
enum charStates {char_move, char_attack, char_dead};
unsigned short xVal, yVal, volDiff, charPos = 38;
unsigned char bestScore;
unsigned int taskNum = 5;
task tasks[5];
char b[3];
char LEnemyCnt = 0, REnemyCnt = 0, enemyDefeatCnt = 0, charDir = 'r', defeat = 0, LPos = -1, RPos = -1;

int joystickTick(int);
int ADCTick(int);
int LCDTick(int);
int charTick(int);
int enemyTick(int);

int main(void) {
    /* Insert DDR and PORT initializations */
    DDRA = 0x00; PINA = 0xFF; //Configure port A as input
    DDRC = 0x00; PINC = 0xFF; // Configure port C as input
    DDRD = 0x03; PORTD = 0xFC; //PD0..1 outputs, PD2..7 inputs
    
    unsigned char i = 0;
    tasks[i].state = ADC_output;
    tasks[i].period = 100;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &ADCTick;
    
    i++;
    tasks[i].state = joystick_wait;
    tasks[i].period = 100;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &joystickTick;

    i++;
    tasks[i].state = char_move;
    tasks[i].period = 200;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &charTick;

    i++;
    tasks[i].state = enemy_move;
    tasks[i].period = 200;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &enemyTick;

    i++;
    tasks[i].state = LCD_start;
    tasks[i].period = 200;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &LCDTick;

    while(!eeprom_is_ready());
    bestScore = eeprom_read_byte(0);
    while(!eeprom_is_ready());
    if(bestScore == 0xFF){
        bestScore = 0;
        eeprom_write_byte(0, 0x00);
    }


    TimerSet(100);
    TimerOn();
    ADC_init();
    nokia_lcd_init();
    /* Insert your solution below */
    while(1){
        for (i = 0; i < taskNum; i++){
            if (tasks[i].elapsedTime >= tasks[i].period){
                tasks[i].state = tasks[i].TickFct(tasks[i].state);
                tasks[i].elapsedTime = 0;
            }
            tasks[i].elapsedTime += 50;
        }
        // LCDTick(LCD_start);
        while (!TimerFlag);
        TimerFlag = 0;
        continue;
    }
    return 1;
}

int ADCTick(int state){
    unsigned short volMax = 0x0000, volMin = 0xFFFF;
    switch(state){
        case ADC_output:
            ADMUX = 0xC0;
            _delay_ms(5);
            xVal = ADC;
            ADMUX = 0xC1;
            _delay_ms(5);
            yVal = ADC;
            ADMUX = 0xC3;
            for(char i = 0; i < 10; i++){
                _delay_ms(5);
                if(ADC > volMax){
                    volMax = ADC;
                }
                else if(ADC < volMin){
                    volMin = ADC;
                }
            }
            volDiff = volMax - volMin;
            break;
        default:
            state = ADC_output;
            break;
    }
    return state;
}

int joystickTick(int state){
    switch(yVal){
        case 543:
            state = joystick_wait;
            break;
        case 1008:
            state = joystick_left;
            break;
        case 28: 
            state = joystick_right;
            break;
        default:
            state = joystick_wait;
            break;
    }
    return state;
}

int LCDTick(int state){
    char tmpStr[20];
    char score[5];
    // char y[5];
    // char final[11];
    static int cnt = 0;
    switch(state){
        case LCD_start:
            nokia_lcd_clear();
            nokia_lcd_set_cursor(0, 0);
            nokia_lcd_set_cursor(10,10);
            nokia_lcd_write_string("Game Start!", 1);
            nokia_lcd_set_cursor(0, 20);
            sprintf(tmpStr, "%d", bestScore);
            nokia_lcd_write_string("Highest: ", 1);
            nokia_lcd_write_string(tmpStr, 1);
            cnt++;
            if(cnt >= 10){
                state = LCD_game;
                cnt = 0;
                break;
            }
            state = LCD_start;
            break;

        case LCD_game:
            if(defeat){
                state = LCD_end;
            }
            if(~PINA & 0x04){
                state = LCD_start;
                enemyDefeatCnt = 0;
                defeat = 0;
                LPos = -1;
                RPos = -1;
                charPos = 38;
                charDir = 'r';
            }
            nokia_lcd_clear();
            nokia_lcd_draw_block(0, 27, 84, 20);
            if(charDir == 'r'){
                nokia_lcd_draw_character(charPos, 20);
            }
            else{
                nokia_lcd_draw_mirrored_character(charPos, 20);
            }
            if(LPos != -1){
                nokia_lcd_draw_enemy(LPos, 20);
            }
            if(RPos != -1){
                nokia_lcd_draw_enemy(RPos, 20);
            }
            if(tasks[2].state == char_attack){
                if(charDir == 'r'){
                    if(RPos != -1){
                        nokia_lcd_draw_block(charPos+9, 22, RPos-charPos-9, 3);
                        RPos = -1;
                        enemyDefeatCnt++;
                    }
                    else{
                        nokia_lcd_draw_block(charPos+9, 22, charPos, 3);
                    }
                }
                else{
                    if(LPos != -1){
                        nokia_lcd_draw_block(LPos+5, 22, charPos-LPos-5, 3);
                        LPos = -1;
                        enemyDefeatCnt++;
                    }
                    else{
                        nokia_lcd_draw_block(0, 22, charPos, 3);
                    }
                
                }
                break;
            }
            strcpy(tmpStr, "Score: ");
            sprintf(score, "%d", enemyDefeatCnt);
            // sprintf(score, "%d", volDiff);
            strcat(tmpStr, score);
            nokia_lcd_set_cursor(0, 0);
            nokia_lcd_write_string(tmpStr, 1);
            break;
        case LCD_end:
                nokia_lcd_clear();
                nokia_lcd_set_cursor(0, 0);
                nokia_lcd_write_string("You Die", 2);
                nokia_lcd_set_cursor(0, 25);
                nokia_lcd_write_string("Press the joystic to restart!", 1);
                if(enemyDefeatCnt > bestScore){
                    bestScore = enemyDefeatCnt;
                    while(!eeprom_is_ready());
                    eeprom_write_byte(0, bestScore);
                }
                if(~PINA & 0x04){
                    state = LCD_start;
                    enemyDefeatCnt = 0;
                    defeat = 0;
                    LPos = -1;
                    RPos = -1;
                    charPos = 38;
                    charDir = 'r';
                }
            break;
        default:
            state = LCD_start;
            break;
    }
    nokia_lcd_render();
    return state;
}


//The character is a 7 x 9 sprite

int charTick(int state){
    switch(state){
        case char_move:
            switch(tasks[1].state){
                case joystick_left:
                    charDir = 'l';
                    if(charPos > 0){
                        charPos -= 2;
                    }
                    break;
                case joystick_right:
                    charDir = 'r';
                    if(charPos < 74){
                        charPos += 2;
                    }
                    break;
                case joystick_wait:
                    break;
                default:
                    break;
            }
            if (volDiff > 30){
                state = char_attack;
            }
            break;
        case char_attack:
            state = char_move;
            break;
        default:
            state = char_move;
            break;
    }
    return state;
}
            

int enemyTick(int state){
    switch(state){
        case enemy_move:
            if(LPos == -1 && RPos == -1){
                if(volDiff & 0x01){
                    LPos = 0;
                }
                else{
                    RPos = 78;
                }
            }
            //Moves enemies, check for collision
            if(LPos != -1){
                if(LPos == 78){
                    LPos = -1;
                    break;
                }
                if(LPos + 5 >= charPos){
                    defeat = 1;
                }
                LPos++;
            }
            if(RPos != -1){
                if(RPos == 0){
                    RPos = -1;
                    break;
                }
                if(RPos - 9 <= charPos){
                    defeat = 1;
                }
                RPos--;
            }
            break;
        default:
            state = enemy_move;
            break;
    }
    return state;
}