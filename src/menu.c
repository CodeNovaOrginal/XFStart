#include <gtk/gtk.h>
#include <gio/gio.h>
#include "menu.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *search;
    GtkWidget *category_list;
    GtkWidget *app_list;
    GList *apps;
} XFStartMenu;

/* -------------------- Utility Functions -------------------- */

static gboolean filter_app(GDesktopAppInfo *appinfo, const gchar *query) {
    if (!query || *query == '\0')
        return TRUE;

    const gchar *name = g_app_info_get_display_name(G_APP_INFO(appinfo));
    if (!name) return FALSE;

    gchar *lower_q = g_ascii_strdown(query, -1);
    gchar *lower_n = g_ascii_strdown(name, -1);

    gboolean match = (strstr(lower_n, lower_q) != NULL);

    g_free(lower_q);
    g_free(lower_n);

    return match;
}

static const gchar* get_category(GDesktopAppInfo *appinfo) {
    const gchar *cats = g_desktop_app_info_get_string(appinfo, "Categories");
    if (!cats) return "Other";

    if (g_strrstr(cats, "Network")) return "Internet";
    if (g_strrstr(cats, "Office")) return "Office";
    if (g_strrstr(cats, "System")) return "System";
    if (g_strrstr(cats, "Utility")) return "Utilities";
    if (g_strrstr(cats, "Development")) return "Development";
    if (g_strrstr(cats, "Graphics")) return "Graphics";
    if (g_strrstr(cats, "Audio") || g_strrstr(cats, "Video")) return "Multimedia";
    if (g_strrstr(cats, "Game")) return "Games";

    return "Other";
}

static void clear_list(GtkListBox *list) {
    GList *rows = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList *l = rows; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(rows);
}

/* -------------------- App List -------------------- */

static void populate_apps(GtkListBox *app_list, GList *apps, const gchar *category, const gchar *query) {
    clear_list(app_list);

    for (GList *l = apps; l; l = l->next) {
        GDesktopAppInfo *info = l->data;

        if (category && g_strcmp0(get_category(info), category) != 0)
            continue;

        if (!filter_app(info, query))
            continue;

        const gchar *name = g_app_info_get_display_name(G_APP_INFO(info));
        const gchar *icon_name = g_app_info_get_icon(G_APP_INFO(info))
            ? g_icon_to_string(g_app_info_get_icon(G_APP_INFO(info)))
            : "application-x-executable";

        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
        GtkWidget *lbl = gtk_label_new(name);

        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);

        gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(row), box);

        gtk_list_box_insert(app_list, row, -1);
        g_object_set_data(G_OBJECT(row), "appinfo", info);
    }

    gtk_widget_show_all(GTK_WIDGET(app_list));
}

static void launch_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    if (!row) return;

    GDesktopAppInfo *info = g_object_get_data(G_OBJECT(row), "appinfo");
    if (!info) return;

    GError *err = NULL;
    if (!g_app_info_launch(G_APP_INFO(info), NULL, NULL, &err)) {
        g_warning("Failed to launch: %s", err->message);
        g_error_free(err);
    }
}

/* -------------------- Categories -------------------- */

static void populate_categories(GtkListBox *category_list, GList *apps) {
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (GList *l = apps; l; l = l->next) {
        GDesktopAppInfo *info = l->data;
        const gchar *cat = get_category(info);

        if (!g_hash_table_contains(seen, cat)) {
            g_hash_table_insert(seen, g_strdup(cat), GINT_TO_POINTER(1));

            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *label = gtk_label_new(cat);
            gtk_container_add(GTK_CONTAINER(row), label);
            gtk_list_box_insert(category_list, row, -1);
        }
    }

    g_hash_table_destroy(seen);
    gtk_widget_show_all(GTK_WIDGET(category_list));
}

static void on_category_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    XFStartMenu *m = user_data;

    const gchar *cat = NULL;
    if (row) {
        GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
        cat = gtk_label_get_text(GTK_LABEL(label));
    }

    const gchar *query = gtk_entry_get_text(GTK_ENTRY(m->search));
    populate_apps(GTK_LIST_BOX(m->app_list), m->apps, cat, query);
}

/* -------------------- Search -------------------- */

static void on_search_changed(GtkEntry *entry, gpointer user_data) {
    XFStartMenu *m = user_data;

    GtkListBoxRow *sel = gtk_list_box_get_selected_row(GTK_LIST_BOX(m->category_list));
    const gchar *cat = NULL;

    if (sel) {
        GtkWidget *label = gtk_bin_get_child(GTK_BIN(sel));
        cat = gtk_label_get_text(GTK_LABEL(label));
    }

    populate_apps(GTK_LIST_BOX(m->app_list), m->apps, cat, gtk_entry_get_text(entry));
}

/* -------------------- Load Apps -------------------- */

static GList* load_apps(void) {
    GList *list = NULL;
    GList *all = g_app_info_get_all();

    for (GList *l = all; l; l = l->next) {
        GAppInfo *ai = l->data;

        if (!g_app_info_should_show(ai))
            continue;

        GDesktopAppInfo *dai = G_DESKTOP_APP_INFO(ai);
        if (!dai)
            continue;

        list = g_list_prepend(list, g_object_ref(dai));
    }

    g_list_free(all);
    return g_list_reverse(list);
}

/* -------------------- System Actions -------------------- */

static GtkWidget* build_power_box(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    GtkWidget *settings = gtk_button_new_with_label("Settings");
    g_signal_connect(settings, "clicked", G_CALLBACK(g_spawn_command_line_async),
                     (gpointer)"xfce4-settings-manager");

    GtkWidget *logout = gtk_button_new_with_label("Log out");
    g_signal_connect(logout, "clicked", G_CALLBACK(g_spawn_command_line_async),
                     (gpointer)"xfce4-session-logout");

    GtkWidget *restart = gtk_button_new_with_label("Restart");
    g_signal_connect(restart, "clicked", G_CALLBACK(g_spawn_command_line_async),
                     (gpointer)"xfce4-session-logout --reboot");

    GtkWidget *shutdown = gtk_button_new_with_label("Shut down");
    g_signal_connect(shutdown, "clicked", G_CALLBACK(g_spawn_command_line_async),
                     (gpointer)"xfce4-session-logout --halt");

    gtk_box_pack_start(GTK_BOX(box), settings, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), logout, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), restart, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), shutdown, FALSE, FALSE, 0);

    return box;
}

/* -------------------- Main Menu Window -------------------- */

GtkWidget* xfstart_menu_new(void) {
    XFStartMenu *m = g_new0(XFStartMenu, 1);

    m->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(m->window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(m->window), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    gtk_window_set_default_size(GTK_WINDOW(m->window), 800, 500);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(m->window), root);

    /* Search bar */
    m->search = gtk_search_entry_new();
    gtk_box_pack_start(GTK_BOX(root), m->search, FALSE, FALSE, 4);

    /* Body layout */
    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(root), body, TRUE, TRUE, 4);

    /* Left: system actions */
    GtkWidget *left = build_power_box();
    gtk_box_pack_start(GTK_BOX(body), left, FALSE, FALSE, 8);

    /* Middle: categories */
    m->category_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(m->category_list), GTK_SELECTION_SINGLE);
    gtk_widget_set_size_request(m->category_list, 200, -1);
    gtk_box_pack_start(GTK_BOX(body), m->category_list, FALSE, TRUE, 0);

    /* Right: app list */
    m->app_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(m->app_list), GTK_SELECTION_SINGLE);
    gtk_box_pack_start(GTK_BOX(body), m->app_list, TRUE, TRUE, 0);

    /* Load apps */
    m->apps = load_apps();
    populate_categories(GTK_LIST_BOX(m->category_list), m->apps);
    populate_apps(GTK_LIST_BOX(m->app_list), m->apps, NULL, NULL);

    /* Signals */
    g_signal_connect(m->category_list, "row-selected", G_CALLBACK(on_category_selected), m);
    g_signal_connect(m->search, "search-changed", G_CALLBACK(on_search_changed), m);
    g_signal_connect(m->app_list, "row-activated", G_CALLBACK(launch_selected), NULL);

    return m->window;
}
