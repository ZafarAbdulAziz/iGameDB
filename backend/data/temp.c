#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>

/* ===================== CONFIG & GLOBALS ===================== */
#define PASSWORD_LENGTH 5

char Data[PASSWORD_LENGTH];
char Master[PASSWORD_LENGTH] = "1234";
char Adnan[PASSWORD_LENGTH] = "6767";
char NewPassword[PASSWORD_LENGTH];

uint8_t data_count = 0;
bool door = false;
bool changePasswordMode = false;
bool enteringNewPassword = false;

/* ===================== LCD DRIVER (4-Bit Mode) ===================== */
#define LCD_PORT PORTC
#define LCD_DDR DDRC
#define RS PC0
#define EN PC1
// LCD Data pins D4-D7 on PC2-PC5

void lcd_pulse(void)
{
    LCD_PORT |= (1 << EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << EN);
    _delay_us(100);
}

void lcd_send_nibble(uint8_t nibble)
{
    // Clear D4-D7 bits (PC2-PC5)
    LCD_PORT &= ~(0x3C);
    // Set new bits
    LCD_PORT |= ((nibble & 0x0F) << 2);
    lcd_pulse();
}

void lcd_cmd(uint8_t cmd)
{
    LCD_PORT &= ~(1 << RS); // RS Low for Command
    lcd_send_nibble(cmd >> 4);
    lcd_send_nibble(cmd & 0x0F);
    _delay_ms(2);
}

void lcd_char(char data)
{
    LCD_PORT |= (1 << RS); // RS High for Data
    lcd_send_nibble(data >> 4);
    lcd_send_nibble(data & 0x0F);
    _delay_us(100);
}

void lcd_string(const char *str)
{
    while (*str)
        lcd_char(*str++);
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
    _delay_ms(2);
}

void lcd_setCursor(uint8_t col, uint8_t row)
{
    uint8_t addr = (row == 0) ? 0x80 : 0xC0;
    lcd_cmd(addr + col);
}

void lcd_init(void)
{
    LCD_DDR = 0x3F; // Set PC0-PC5 as output
    _delay_ms(50);

    // Initialization sequence for 4-bit mode
    LCD_PORT &= ~(1 << RS);
    lcd_send_nibble(0x03);
    _delay_ms(5);
    lcd_send_nibble(0x03);
    _delay_us(150);
    lcd_send_nibble(0x03);
    lcd_send_nibble(0x02); // Set to 4-bit mode

    lcd_cmd(0x28); // 2 lines, 5x7 matrix
    lcd_cmd(0x0C); // Display ON, Cursor OFF
    lcd_cmd(0x06); // Increment cursor
    lcd_clear();
}

/* ===================== KEYPAD DRIVER ===================== */
// ROWS: PD0-PD3, COLS: PD4-PD7
const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

void keypad_init(void)
{
    DDRD = 0x0F;  // Rows output, Cols input
    PORTD = 0xF0; // Pull-ups on Cols
}

char keypad_getKey(void)
{
    for (uint8_t r = 0; r < 4; r++)
    {
        PORTD = ~(1 << r); // Set current row LOW
        _delay_us(10);     // Stabilize

        uint8_t pins = PIND;

        for (uint8_t c = 0; c < 4; c++)
        {
            // Check if col bit is LOW
            if (!(pins & (1 << (c + 4))))
            {
                // Simple debounce / wait for release logic could go here
                // For now, return key and handle debounce in main logic
                _delay_ms(200); // Small delay to mimic Arduino library debounce
                return keymap[r][c];
            }
        }
    }
    return 0; // No key pressed
}

/* ===================== SERVO DRIVER ===================== */
// Mimicking: myservo.attach(9, 2000, 2400);
// Pin 9 is PB1 (OC1A) on Arduino Uno
// Timer1 (16-bit) Fast PWM, ICR1 top. Prescaler 8.
// 1 tick = 0.5us.
// 2000us = 4000 ticks.
// 2400us = 4800 ticks.

void servo_init(void)
{
    DDRB |= (1 << PB1); // Set PB1 as output
    // Fast PWM mode 14 (WGM13:0 = 1110), Non-inverting (COM1A1 = 1)
    // Prescaler 8 (CS11 = 1) -> 16MHz/8 = 2MHz ticks (0.5us)
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    ICR1 = 39999; // 20ms period (50Hz)
}

void servo_write_angle(int angle)
{
    // Map 0-180 degrees to 4000-4800 ticks (based on Arduino code values)
    // Slope: (4800 - 4000) / 180 = 4.44 ticks per degree
    uint16_t pulse = 4000 + (angle * 4.44);
    OCR1A = pulse;
}

void ServoClose(void)
{
    for (int pos = 90; pos >= 0; pos -= 10)
    {
        servo_write_angle(pos);
        _delay_ms(15); // Default Arduino servo speed
    }
}

void ServoOpen(void)
{
    for (int pos = 0; pos <= 90; pos += 10)
    {
        servo_write_angle(pos);
        _delay_ms(15);
    }
}

/* ===================== ANIMATIONS & HELPERS ===================== */
void loading(const char *msg)
{
    lcd_setCursor(0, 1);
    lcd_string(msg);
    for (int i = 0; i < 9; i++)
    {
        _delay_ms(250);
        lcd_char('.');
    }
}

void clearData(void)
{
    while (data_count != 0)
    {
        Data[data_count--] = 0;
    }
}

void displaySpecial(void)
{
    const char message[] = "SIR ADNAN MERI JIND MERI JAAN";
    int messageLength = strlen(message);

    lcd_clear();

    // Scroll animation
    for (int pos = 16; pos > -messageLength; pos--)
    {
        lcd_clear();

        // Handle off-screen logic manually for AVR
        if (pos >= 0)
        {
            lcd_setCursor(pos, 0);
            lcd_string(message);
        }
        else
        {
            // String has started scrolling off left side
            lcd_setCursor(0, 0);
            lcd_string(message + (-pos));
        }

        _delay_ms(400);
    }
    _delay_ms(2000);
    lcd_clear();
}

/* ===================== LOGIC FUNCTIONS ===================== */

void ChangePassword(void)
{
    if (!enteringNewPassword)
    {
        // Step 1: Verify Current Password
        lcd_setCursor(0, 0);
        lcd_string("Enter Current");
        lcd_setCursor(0, 1);
        lcd_string("Password:");

        char key = keypad_getKey();

        if (key == '#')
        {
            lcd_clear();
            lcd_string("Cancelled");
            _delay_ms(1500);
            lcd_clear();
            clearData();
            changePasswordMode = false;
            return;
        }

        if (key && key != '*')
        {
            Data[data_count] = key;
            lcd_setCursor(9 + data_count, 1);
            lcd_char('*');
            data_count++;
        }

        if (data_count == PASSWORD_LENGTH - 1)
        {
            if (strcmp(Data, Master) == 0)
            {
                lcd_clear();
                lcd_string("Enter New");
                lcd_setCursor(0, 1);
                lcd_string("Password:");
                _delay_ms(2000);
                lcd_clear();
                clearData();
                enteringNewPassword = true;
            }
            else
            {
                lcd_clear();
                lcd_string("Wrong Password");
                _delay_ms(2000);
                lcd_clear();
                clearData();
                changePasswordMode = false;
            }
        }
    }
    else
    {
        // Step 2: Enter New Password
        lcd_setCursor(0, 0);
        lcd_string("New Password:");

        char key = keypad_getKey();

        if (key == '#')
        {
            lcd_clear();
            lcd_string("Cancelled");
            _delay_ms(1500);
            lcd_clear();
            clearData();
            changePasswordMode = false;
            enteringNewPassword = false;
            return;
        }

        if (key && key != '*')
        {
            NewPassword[data_count] = key;
            lcd_setCursor(data_count, 1);
            lcd_char('*');
            data_count++;
        }

        if (data_count == PASSWORD_LENGTH - 1)
        {
            // Save logic
            for (int i = 0; i < PASSWORD_LENGTH; i++)
            {
                Master[i] = NewPassword[i];
            }
            lcd_clear();
            lcd_string("Password Changed");
            _delay_ms(2000);
            lcd_clear();
            clearData();
            changePasswordMode = false;
            enteringNewPassword = false;
        }
    }
}

void Open(void)
{
    lcd_setCursor(0, 0);
    lcd_string("Enter Password");

    char key = keypad_getKey();

    if (key == '#')
    {
        lcd_clear();
        clearData();
        return;
    }

    if (key == '*')
    {
        lcd_clear();
        changePasswordMode = true;
        enteringNewPassword = false;
        clearData();
        return;
    }

    if (key)
    {
        Data[data_count] = key;
        lcd_setCursor(data_count, 1);
        lcd_char(key);
        data_count++;
    }

    if (data_count == PASSWORD_LENGTH - 1)
    {
        if (strcmp(Data, Master) == 0)
        {
            lcd_clear();
            ServoOpen();
            lcd_string(" Door is Open ");
            door = true;
            _delay_ms(5000);

            loading("Waiting");

            lcd_clear();
            lcd_string(" Time is up! ");
            _delay_ms(1000);
            ServoClose();
            door = false;
        }
        else
        {
            if (strcmp(Data, Adnan) == 0)
            {
                displaySpecial();
            }
            lcd_clear();
            lcd_string(" Wrong Password ");
            door = false;
        }
        _delay_ms(1000);
        lcd_clear();
        clearData();
    }
}

/* ===================== MAIN ===================== */
int main(void)
{
    // Setup
    servo_init();
    ServoClose();
    lcd_init();
    keypad_init();

    lcd_string("Protected Door");
    loading("Loading");
    lcd_clear();

    while (1)
    {
        if (door == true)
        {
            char key = keypad_getKey();
            if (key == '#')
            {
                lcd_clear();
                ServoClose();
                lcd_string("Door is closed");
                _delay_ms(3000);
                door = false;
                lcd_clear();
            }
        }
        else if (changePasswordMode)
        {
            ChangePassword();
        }
        else
        {
            Open();
        }
    }
}