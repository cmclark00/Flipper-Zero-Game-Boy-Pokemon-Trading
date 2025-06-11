#ifndef UI_H
#define UI_H

typedef enum {
    UI_STATUS_IDLE,
    UI_STATUS_WORKING,
    UI_STATUS_ERROR
} ui_status_t;

void ui_init(void);
int ui_button_pressed(void);
void ui_set_status(ui_status_t status);

#endif // UI_H 