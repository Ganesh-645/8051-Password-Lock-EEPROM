#include <reg51.h>
#include <string.h>

// ================= LCD =================
sbit LCD_RS = P3^0;
sbit LCD_EN = P3^1;

// ================= KEYPAD =================
sbit R0 = P1^0;
sbit R1 = P1^1;
sbit R2 = P1^2;
sbit R3 = P1^3;

sbit C0 = P1^4;
sbit C1 = P1^5;
sbit C2 = P1^6;
sbit C3 = P1^7;

// ================= EEPROM =================
sbit SDA = P3^4;
sbit SCL = P3^5;

// ================= SYSTEM =================
char pass[5];
char stored_pass[5];
unsigned char idx = 0;
unsigned char attempts = 0;
bit locked = 0;

#define EEPROM_ATTEMPT_ADDR  0x20

// ================= MODES =================
#define MODE_NORMAL   0
#define MODE_OLDPASS  1
#define MODE_NEWPASS  2

unsigned char mode = MODE_NORMAL;

// ================= BUZZER =================
sbit BUZZER = P3^6;   // Buzzer control pin


// ================= DELAY =================
void delay_ms(unsigned int ms){
    unsigned int i,j;
    for(i=0;i<ms;i++)
        for(j=0;j<1075;j++);
}

void delay_1s(void){
    unsigned int i;
    for(i=0;i<400;i++) delay_ms(1);
}

// ================= BUZZER =================
void buzzer_on(void){
    BUZZER = 1;   // Turn buzzer ON
}

void buzzer_off(void){
    BUZZER = 0;   // Turn buzzer OFF
}

void buzzer_beep(unsigned int ms){
    buzzer_on();
    delay_ms(ms);
    buzzer_off();
}


// ================= LCD =================
void lcd_cmd(unsigned char cmd){
    P2 = cmd;
    LCD_RS = 0;
    LCD_EN = 1;
    delay_ms(2);
    LCD_EN = 0;
}

void lcd_data(unsigned char ch){
    P2 = ch;
    LCD_RS = 1;
    LCD_EN = 1;
    delay_ms(2);
    LCD_EN = 0;
}

void lcd_print(char *s){
    while(*s) lcd_data(*s++);
}

void lcd_init(){
    delay_ms(20);
    lcd_cmd(0x38);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
}

// ================= KEYPAD =================
void keypad_init(void){
    R0 = R1 = R2 = R3 = 1;
}

char keypad_scan(void){
    char key = 0;

    R0=0; R1=R2=R3=1;
    if(C0==0) key='7'; else if(C1==0) key='8'; else if(C2==0) key='9'; else if(C3==0) key='D';

    R1=0; R0=R2=R3=1;
    if(C0==0) key='4'; else if(C1==0) key='5'; else if(C2==0) key='6'; else if(C3==0) key='C';

    R2=0; R0=R1=R3=1;
    if(C0==0) key='1'; else if(C1==0) key='2'; else if(C2==0) key='3'; else if(C3==0) key='B';

    R3=0; R0=R1=R2=1;
    if(C0==0) key='*'; else if(C1==0) key='0'; else if(C2==0) key='#'; else if(C3==0) key='A';

    R0 = R1 = R2 = R3 = 1;
    return key;
}

// ================= EEPROM =================
void i2c_start(void){ SDA=1; SCL=1; delay_ms(1); SDA=0; SCL=0; }
void i2c_stop(void){ SDA=0; SCL=1; delay_ms(1); SDA=1; }

void i2c_write(unsigned char d){
    unsigned char i;
    for(i=0;i<8;i++){
        SDA=(d&0x80);
        SCL=1; delay_ms(1); SCL=0;
        d<<=1;
    }
    SDA=1; SCL=1; delay_ms(1); SCL=0;
}

unsigned char i2c_read(void){
    unsigned char i,d=0;
    SDA=1;
    for(i=0;i<8;i++){
        SCL=1; delay_ms(1);
        d=(d<<1)|SDA;
        SCL=0;
    }
    return d;
}

void eeprom_write(unsigned char a,unsigned char d){
    i2c_start(); i2c_write(0xA0);
    i2c_write(a); i2c_write(d);
    i2c_stop(); delay_ms(10);
}

unsigned char eeprom_read(unsigned char a){
    unsigned char d;
    i2c_start(); i2c_write(0xA0); i2c_write(a);
    i2c_start(); i2c_write(0xA1);
    d=i2c_read(); i2c_stop();
    return d;
}

// ================= EEPROM HELPERS =================
void eeprom_init_password(void){
    unsigned char i;
    char def[4]={'2','0','0','5'};
    if(eeprom_read(0x10)!=0xAA){
        for(i=0;i<4;i++) eeprom_write(i,def[i]);
        eeprom_write(0x10,0xAA);
    }
}

void read_stored_password(void){
    unsigned char i;
    for(i=0;i<4;i++) stored_pass[i]=eeprom_read(i);
    stored_pass[4]='\0';
}

// ================= LOCK (BLOCKING, SIMPLE) =================
void lock_system(void){
    unsigned char i;
    locked = 1;

    lcd_cmd(0x01);
    lcd_print("LOCKED");
    lcd_cmd(0xC0);
    lcd_print("WAIT 30 SEC");

    for(i=0;i<10;i++) delay_1s();

    attempts = 0;
    locked = 0;

    lcd_cmd(0x01);
    lcd_print("ENTER PASS:");
    lcd_cmd(0xC0);
}

// ================= MAIN =================
void main(void){
    char key,i;

    lcd_init();
    keypad_init();

    eeprom_init_password();
    read_stored_password();

    lcd_print("ENTER PASS:");
    lcd_cmd(0xC0);

    while(1){

        if(locked) continue;

        key = keypad_scan();

        // ENTER CHANGE PASSWORD MODE
        if(key=='D' && mode==MODE_NORMAL){
            mode = MODE_OLDPASS;
            idx = 0;
            lcd_cmd(0x01);
            lcd_print("OLD PASSWORD:");
            lcd_cmd(0xC0);
            continue;
        }

        // DIGITS
        if(key>='0' && key<='9' && idx<4){
            pass[idx++] = key;
            lcd_data('*');
            delay_ms(300);
        }

        // ENTER
        else if(key=='#'){
            pass[idx]='\0';

            if(mode==MODE_NORMAL){
                if(strcmp(pass,stored_pass)==0){
                    attempts=0;
                    lcd_cmd(0x01);
                    lcd_print("ACCESS GRANTED");
                } else {
									attempts++;

									lcd_cmd(0x01);
									lcd_print("WRONG PASS");

									buzzer_beep(1000);   // ?? short beep for wrong password

									if(attempts >= 3){
											buzzer_beep(2000);  // ?? long beep before lock
											lock_system();
									}
							}

            }

            else if(mode==MODE_OLDPASS){
                if(strcmp(pass,stored_pass)==0){
                    mode = MODE_NEWPASS;
                    idx = 0;
                    lcd_cmd(0x01);
                    lcd_print("NEW PASSWORD:");
                    lcd_cmd(0xC0);
                    continue;
                } else {
                    mode = MODE_NORMAL;
                    lcd_cmd(0x01);
                    lcd_print("WRONG OLD PASS");
                }
            }

       else if(mode==MODE_NEWPASS){

    if(idx < 4){
        lcd_cmd(0x01);
        lcd_print("4 DIGITS ONLY");
        delay_ms(1000);

        // Stay in NEWPASS mode
        lcd_cmd(0x01);
        lcd_print("NEW PASSWORD:");
        lcd_cmd(0xC0);
        continue;   // ? VERY IMPORTANT
    }

    // Save new password
    for(i=0;i<4;i++){
        eeprom_write(i, pass[i]);
        stored_pass[i] = pass[i];
    }
    stored_pass[4] = '\0';

    lcd_cmd(0x01);
    lcd_print("PASS UPDATED");
    delay_ms(1000);

    mode = MODE_NORMAL;   // Exit only after SUCCESS
}



            idx = 0;
            delay_ms(1000);
            lcd_cmd(0x01);
            lcd_print("ENTER PASS:");
            lcd_cmd(0xC0);
        }

        // CLEAR
        else if(key=='*'){
            idx = 0;
            mode = MODE_NORMAL;
            lcd_cmd(0x01);
            lcd_print("CLEARED");
            delay_ms(800);
            lcd_cmd(0x01);
            lcd_print("ENTER PASS:");
            lcd_cmd(0xC0);
        }
    }
}
