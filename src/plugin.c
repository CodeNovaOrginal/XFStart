#include <libxfce4panel/libxfce4panel.h>
#include <gtk/gtk.h>
#include "menu.h"

static GtkWidget *popup = NULL;

static void position_popup(GtkWidget *button, GtkWidget *win) {
    gint x = 0, y = 0;
    GdkWindow *btn_win = gtk_widget_get_window(button);
    if (!btn_win) return;
    gdk_window_get_origin(btn_win, &x, &y);

    GtkAllocation alloc;
    gtk_widget_get_allocation(button, &alloc);
    gtk_window_move(GTK_WINDOW(win), x, y + alloc.height);
}

static void on_button_clicked(GtkWidget *btn, gpointer user_data) {
    if (!popup)
        popup = xfstart_menu_new();

    if (gtk_widget_get_visible(popup)) {
        gtk_widget_hide(popup);
    } else {
        position_popup(btn, popup);
        gtk_widget_show_all(popup);
    }
}

static void construct(XfcePanelPlugin *plugin) {
    GtkWidget *button = gtk_button_new();
    GtkWidget *image = gtk_image_new_from_icon_name("view-app-grid-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), image);

    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), NULL);

    xfce_panel_plugin_add_action_widget(plugin, button);
    gtk_container_add(GTK_CONTAINER(plugin), button);
    gtk_widget_show_all(GTK_WIDGET(plugin));
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(construct)
