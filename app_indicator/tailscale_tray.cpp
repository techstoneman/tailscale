// TAILSCALE APP INDICATOR BY Lord_stoneman
// requirements:
//  - build-essential , pkg-config , libgtk-3-dev , libayatana-appindicator3-dev , tailscale
// disable sudo requirement for tailscale:
//  - sudo tailscale set --operator=$USER
// COMPILE WITH: g++ -std=c++17 tailscale_tray.cpp -o tailscale_tray $(pkg-config --cflags --libs gtk+-3.0 ayatana-appindicator3-0.1)

#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#define PATH_ICON "/home/USER/tailscale/"

#define MAX_RESULTS 128
#define MAX_NAME_LEN 256
int get_tailscale_dash_hosts(char results[][MAX_NAME_LEN], int max_results) {
    FILE *fp = popen("tailscale status", "r");
    if (!fp) return -1;

    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < max_results) {
        char *tokens[32];
        int ntokens = 0;

        char *saveptr;
        char *tok = strtok_r(line, " \t\n", &saveptr);

        while (tok && ntokens < 32) {
            tokens[ntokens++] = tok;
            tok = strtok_r(NULL, " \t\n", &saveptr);
        }

        if (ntokens >= 2 && strcmp(tokens[ntokens - 1], "-") == 0) {
            strncpy(results[count], tokens[1], MAX_NAME_LEN - 1);
            results[count][MAX_NAME_LEN - 1] = '\0';
            count++;
        }
    }

    pclose(fp);
    return count;
}


class TailscaleIndicator {
private:
    AppIndicator *indicator;
    GtkWidget *menu;

    const std::string icon_on = std::string(PATH_ICON) + "icon_on.png";
    const std::string icon_off = std::string(PATH_ICON) + "icon_off.png";

public:
    TailscaleIndicator()
    {
        indicator = app_indicator_new(
            "tailscale-status",
            icon_off.c_str(),
            APP_INDICATOR_CATEGORY_APPLICATION_STATUS
        );

        app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

        menu = gtk_menu_new();
        app_indicator_set_menu(indicator, GTK_MENU(menu));

        rebuild_menu();
        update_ui();

        // refresh every 10 seconds
        g_timeout_add(10000, refresh_cb, this);
    }

    static bool is_running()
    {
        FILE *pipe = popen("tailscale status 2>&1", "r");
        if (!pipe) return false;

        char buffer[128];
        std::string result;

        while (fgets(buffer, sizeof(buffer), pipe))
            result += buffer;

        pclose(pipe);

        for (auto &c : result)
            c = tolower(c);

        return result.find("stopped") == std::string::npos;
    }

    void update_ui()
    {
        if (is_running()) {
            app_indicator_set_icon_full(indicator, icon_on.c_str(), "Connected");
        } else {
            app_indicator_set_icon_full(indicator, icon_off.c_str(), "Disconnected");
        }
    }

    void clear_menu()
    {
        GList *children =
            gtk_container_get_children(GTK_CONTAINER(menu));

        for (GList *l = children; l != NULL; l = l->next)
            gtk_widget_destroy(GTK_WIDGET(l->data));

        g_list_free(children);
    }

    void rebuild_menu() {
        clear_menu();

        GtkWidget *toggle_item =
            gtk_menu_item_new_with_label("Toggle Tailscale");

        g_signal_connect(toggle_item, "activate", G_CALLBACK(toggle_cb), this);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), toggle_item);

        char hosts[MAX_RESULTS][MAX_NAME_LEN];
        int n = get_tailscale_dash_hosts(hosts, MAX_RESULTS);

        char markup[512];
        int offset = 0;

        for (int i = 0; i < n; i++) {
            const char* str;
            if (i+1 == n) str = "🟢 ‒ %s"; else str = "🟢 ‒ %s\n";
            offset += snprintf(markup + offset, sizeof(markup) - offset, str, hosts[i]);
        }

        GtkWidget *showonline = gtk_menu_item_new_with_label(markup);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), showonline);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");

        g_signal_connect(quit_item, "activate", G_CALLBACK(quit_cb), nullptr);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

        gtk_widget_show_all(menu);
    }

    static void toggle_cb(GtkMenuItem *, gpointer data)
    {
        auto *self = static_cast<TailscaleIndicator *>(data);

        if (is_running())
            std::system("tailscale down");
        else
            std::system("tailscale up");

        self->update_ui();
        self->rebuild_menu();
    }

    static void quit_cb(GtkMenuItem *, gpointer)
    {
        gtk_main_quit();
    }

    static gboolean refresh_cb(gpointer data)
    {
        if (!data)
            return FALSE;

        auto *self = static_cast<TailscaleIndicator *>(data);

        self->update_ui();
        self->rebuild_menu();

        return TRUE;
    }
};

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    TailscaleIndicator app;

    gtk_main();
    return 0;
}
