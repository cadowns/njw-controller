SET(SOURCES ui_comp_txtbtn.c
    ui_comp.c
    ui_HomeScreen.c
    ui_SettingsScreen.c
    ui_TextEntry.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c)

add_library(ui ${SOURCES})
