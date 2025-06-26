#include "display.h"
#include "menu.h"
#include "buttons.h"
#include <string.h>
#include <lvgl.h>

extern uint16_t screen_buffer[240][240];  /* already defined elsewhere */

void run_old_menu(void)
{
    /* suspend LVGL refresh so we can draw raw pixels */
    lv_disp_t * d = lv_disp_get_default();
    lv_disp_suspend(d);

    /* main loop for the old menu */
    for (;;)
    {
        update_buttons();                      /* your normal input task */

        /* draw one frame of the text UI */
        memset(screen_buffer, 0x01, sizeof(screen_buffer));
        do_menu();                             /* old code :contentReference[oaicite:5]{index=5} */
        st7796_blit_screen(240*240, (uint16_t*)screen_buffer);

        /* exit when the user presses <centre> **while holding** <left>   */
        if ((buttons_pulsed & BUTTON_CENTER) && (buttons_down & BUTTON_LEFT))
            break;
    }

    /* clean up - swallow the key that closed the menu so LVGL doesn’t see it */
    buttons_clear_state();                     /* helper in main.c :contentReference[oaicite:6]{index=6} */
    display_reset_keypad();                    /* clears LVGL indev   :contentReference[oaicite:7]{index=7} */

    lv_disp_resume(d);                         /* give the panel back to LVGL */
}
