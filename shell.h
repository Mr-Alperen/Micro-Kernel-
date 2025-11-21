#ifndef SHELL_H
#define SHELL_H

// Shell'in ana döngüsünü başlatır
void shell_main_loop();

// Klavye sürücüsünden bir tuş vuruşu alır ve işler
void shell_handle_keypress(char c);

#endif