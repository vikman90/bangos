#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
int  keyboard_has_char(void);
char keyboard_getc(void);
int  console_has_char(void);
char console_getchar(void);

#endif /* KEYBOARD_H */
