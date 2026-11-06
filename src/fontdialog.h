/**
 * @author: Ahmad Odeh
 * @date: 7th Nov, 2026
 *
**/
/*
 * fontdialog: View > Fonts... Two GtkFontChoosers (code font, interface
 * font) with sizes, a live preview of Lunar, C and Python code, and the
 * change applied to the editor while you pick. OK saves, Cancel puts
 * back what was there, "Reset to Default" goes back to the theme's fonts.
 */
#ifndef PHOTON_FONTDIALOG_H
#define PHOTON_FONTDIALOG_H

#include "app.h"

void photon_font_dialog_run(PhotonApp *app);

#endif
