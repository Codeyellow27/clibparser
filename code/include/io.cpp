//
// Project: clibparser
// Created by bajdcc
//

// IO输入与输出

// 输出部分
int put_char(char c) {
    c;
    interrupt 0;
}
int put_string(char *text) {
    while (put_char(*text++));
}
int put_int(int number) {
    number;
    interrupt 1;
}
int put_hex(int number) {
    number;
    interrupt 2;
}
float put_float(float number) {
    number;
    interrupt 4;
}
double put_double(double number) {
    number;
    interrupt 6;
}
long put_long(long number) {
    number;
    interrupt 7;
}
int input_character(char c) {
    c;
    interrupt 8;
}
int input_string(char *text) {
    while (input_character(*text++));
}

// 输入部分
enum input_special {
    INPUT_BEGIN = -9,
    INPUT_UP = -10,
    INPUT_DOWN = -11,
    INPUT_LEFT = -12,
    INPUT_RIGHT = -13,
};
int input_char() {
    interrupt 11;
}
int input_lock() {
    interrupt 10;
}
int input_unlock() {
    interrupt 12;
}
int input_state() {
    interrupt 13;
}
int input(char *text, int len) {
    int i, c;
    int state = input_lock();
    input_string(text);
    for (i = 0; i < len && ((c = input_char()) != -1); ++i) {
        if (c <= INPUT_BEGIN) {
            input_unlock();
            text[i] = '\0';
            return c;
        }
        text[i] = c;
    }
    input_unlock();
    text[i++] = '\0';
    return c;
}

// 界面部分
int resize(int rows, int cols) {
    (rows << 16) | cols;
    interrupt 20;
}
int set_bg(int r, int g, int b) {
    put_char('\033');
    put_char('B');
    put_hex((0xff << 24) | (b << 16) | (g << 8) | r);
    put_char('\033');
}
int set_fg(int r, int g, int b) {
    put_char('\033');
    put_char('F');
    put_hex((0xff << 24) | (b << 16) | (g << 8) | r);
    put_char('\033');
}
int gui_cfg(char c) {
    put_char('\033');
    put_char('S');
    put_char(c);
    put_char('\033');
}
int newline() {
    gui_cfg('0');
}
int save_bg() {
    gui_cfg('1');
}
int save_fg() {
    gui_cfg('2');
}
int restore_bg() {
    gui_cfg('3');
}
int restore_fg() {
    gui_cfg('4');
}
