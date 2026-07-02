#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 200
#define MAX_NAME_LEN 64
#define MAX_PATH_LEN 256

typedef struct MenuItem {
    char name[MAX_NAME_LEN];
    double price;
    struct MenuItem *parent;
    struct MenuItem **children;
    int child_count;
    int child_capacity;
    int is_category;
} MenuItem;

typedef struct {
    MenuItem *root;
    MenuItem *selected_item;
    GtkWidget *window;
    GtkWidget *tree_view;
    GtkWidget *info_label;
    GtkWidget *name_entry;
    GtkWidget *price_entry;
    GtkWidget *parent_combo;
    GtkCssProvider *css_provider;
} AppData;

static MenuItem* menu_item_create(const char *name, double price, int is_category) {
    MenuItem *item = g_new0(MenuItem, 1);
    strncpy(item->name, name, MAX_NAME_LEN - 1);
    item->name[MAX_NAME_LEN - 1] = '\0';
    item->price = price;
    item->is_category = is_category;
    item->child_capacity = 10;
    item->children = g_new0(MenuItem*, item->child_capacity);
    return item;
}

static void menu_item_add_child(MenuItem *parent, MenuItem *child) {
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = g_realloc(parent->children, 
                                      sizeof(MenuItem*) * parent->child_capacity);
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
}

static MenuItem* find_item_by_path(MenuItem *root, const char *path) {
    if (!path || strlen(path) == 0) return root;
    
    char *path_copy = g_strdup(path);
    char *token = strtok(path_copy, "/");
    MenuItem *current = root;
    
    while (token != NULL) {
        gboolean found = FALSE;
        for (int i = 0; i < current->child_count; i++) {
            if (strcmp(current->children[i]->name, token) == 0) {
                current = current->children[i];
                found = TRUE;
                break;
            }
        }
        if (!found) {
            g_free(path_copy);
            return NULL;
        }
        token = strtok(NULL, "/");
    }
    
    g_free(path_copy);
    return current;
}

static void load_default_menu(MenuItem *root) {
    MenuItem *coffee = menu_item_create("Кофе", 0.0, TRUE);
    MenuItem *tea = menu_item_create("Чай", 0.0, TRUE);
    MenuItem *drinks = menu_item_create("Напитки", 0.0, TRUE);
    MenuItem *desserts = menu_item_create("Десерты", 0.0, TRUE);
    
    menu_item_add_child(root, coffee);
    menu_item_add_child(root, tea);
    menu_item_add_child(root, drinks);
    menu_item_add_child(root, desserts);
    
    MenuItem *espresso = menu_item_create("Эспрессо", 90.0, FALSE);
    MenuItem *americano = menu_item_create("Американо", 110.0, FALSE);
    MenuItem *cappuccino = menu_item_create("Капуччино", 140.0, FALSE);
    MenuItem *latte = menu_item_create("Латте", 150.0, FALSE);
    MenuItem *mocha = menu_item_create("Моккачино", 160.0, FALSE);
    
    menu_item_add_child(coffee, espresso);
    menu_item_add_child(coffee, americano);
    menu_item_add_child(coffee, cappuccino);
    menu_item_add_child(coffee, latte);
    menu_item_add_child(coffee, mocha);
    
    MenuItem *black_tea = menu_item_create("Чай чёрный", 80.0, FALSE);
    MenuItem *green_tea = menu_item_create("Чай зелёный", 80.0, FALSE);
    MenuItem *fruit_tea = menu_item_create("Чай фруктовый", 90.0, FALSE);
    
    menu_item_add_child(tea, black_tea);
    menu_item_add_child(tea, green_tea);
    menu_item_add_child(tea, fruit_tea);
    
    MenuItem *lemonade = menu_item_create("Лимонад", 100.0, FALSE);
    MenuItem *juice = menu_item_create("Сок апельсиновый", 120.0, FALSE);
    MenuItem *water = menu_item_create("Вода минеральная", 60.0, FALSE);
    
    menu_item_add_child(drinks, lemonade);
    menu_item_add_child(drinks, juice);
    menu_item_add_child(drinks, water);
    
    MenuItem *milkshake = menu_item_create("Молочный коктейль", 180.0, FALSE);
    
    menu_item_add_child(desserts, milkshake);
}

static gboolean load_css(AppData *app) {
    GError *error = NULL;
    app->css_provider = gtk_css_provider_new();
    
    gchar *css_path = g_build_filename(g_get_current_dir(), "style.css", NULL);
    gtk_css_provider_load_from_path(app->css_provider, css_path, &error);
    g_free(css_path);
    
    if (error != NULL) {
        g_warning("CSS load error: %s", error->message);
        g_error_free(error);
        return FALSE;
    }
    
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(app->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    
    return TRUE;
}

static void on_tree_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *name;
        gdouble price;
        gboolean is_category;
        gchar *path_str;
        
        gtk_tree_model_get(model, &iter, 
                           0, &name,
                           1, &price,
                           2, &is_category,
                           3, &path_str,
                           -1);
        
        char info[512];
        if (is_category) {
            snprintf(info, sizeof(info), "Категория: %s\nНажмите для просмотра подкатегорий", name);
        } else {
            snprintf(info, sizeof(info), "Выбрано: %s\nЦена: %.2f руб.", name, price);
        }
        
        gtk_label_set_text(GTK_LABEL(app->info_label), info);
        
        g_free(name);
        g_free(path_str);
    }
}

static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                             GtkTreeViewColumn *column, gpointer user_data) {
    (void)column;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gboolean is_category;
        gchar *name;
        gchar *path_str;
        
    gtk_tree_model_get(model, &iter,
                           0, &name,
                           2, &is_category,
                           3, &path_str,
                           -1);
        
    if (is_category) {
        if (gtk_tree_view_row_expanded(tree_view, path)) {
            gtk_tree_view_collapse_row(tree_view, path);
        } else {
            gtk_tree_view_expand_to_path(tree_view, path);
        }
    }
    
    g_free(name);
    g_free(path_str);
}

static GtkTreeStore* create_tree_store() {
    return gtk_tree_store_new(4, G_TYPE_STRING, G_TYPE_DOUBLE, 
                              G_TYPE_BOOLEAN, G_TYPE_STRING);
}

static void populate_tree_store(GtkTreeStore *store, MenuItem *item, 
                                GtkTreeIter *parent_iter, const char *path) {
    char new_path[MAX_PATH_LEN];
    if (path) {
        snprintf(new_path, sizeof(new_path), "%s/%s", path, item->name);
    } else {
        snprintf(new_path, sizeof(new_path), "%s", item->name);
    }
    
    GtkTreeIter iter;
    gtk_tree_store_append(store, &iter, parent_iter);
    gtk_tree_store_set(store, &iter,
                       0, item->name,
                       1, item->price,
                       2, item->is_category,
                       3, new_path,
                       -1);
    
    for (int i = 0; i < item->child_count; i++) {
        populate_tree_store(store, item->children[i], &iter, new_path);
    }
}

static void refresh_tree_view(AppData *app) {
    GtkTreeStore *store = create_tree_store();
    populate_tree_store(store, app->root, NULL, NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(app->tree_view), GTK_TREE_MODEL(store));
    g_object_unref(store);
}

static void add_items_recursive(GtkListStore *list_store, GtkTreeIter *parent_iter,
                                MenuItem *item, const char *path) {
    char item_path[MAX_PATH_LEN];
    snprintf(item_path, sizeof(item_path), "%s%s%s", 
             path ? path : "", path ? "/" : "", item->name);
    
    if (item->is_category) {
        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, item_path, 1, item->name, -1);
        
        for (int i = 0; i < item->child_count; i++) {
            add_items_recursive(list_store, &iter, item->children[i], item_path);
        }
    }
}

static void update_parent_combo(AppData *app) {
    GtkListStore *list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    GtkTreeIter iter;
    
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, "", 1, "(корневой уровень)", -1);
    
    for (int i = 0; i < app->root->child_count; i++) {
        add_items_recursive(list_store, NULL, app->root->children[i], NULL);
    }
    
    gtk_combo_box_set_model(GTK_COMBO_BOX(app->parent_combo), GTK_TREE_MODEL(list_store));
    g_object_unref(list_store);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(app->parent_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(app->parent_combo), renderer,
                                   "text", 1, NULL);
}

static void on_add_item_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *app = (AppData *)user_data;
    
    const char *name = gtk_entry_get_text(GTK_ENTRY(app->name_entry));
    const char *price_str = gtk_entry_get_text(GTK_ENTRY(app->price_entry));
    gchar *parent_path = NULL;
    
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(app->parent_combo), &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(app->parent_combo));
        gtk_tree_model_get(model, &iter, 0, &parent_path, -1);
    }
    
    if (strlen(name) == 0) {
        gtk_label_set_text(GTK_LABEL(app->info_label), "Ошибка: введите название!");
        return;
    }
    
    double price = 0.0;
    if (strlen(price_str) > 0) {
        price = atof(price_str);
    }
    
    MenuItem *parent;
    if (parent_path && strlen(parent_path) > 0) {
        parent = find_item_by_path(app->root, parent_path);
    } else {
        parent = app->root;
    }
    
    if (!parent || !parent->is_category) {
        gtk_label_set_text(GTK_LABEL(app->info_label), "Ошибка: выберите категорию!");
        g_free(parent_path);
        return;
    }
    
    MenuItem *new_item = menu_item_create(name, price, price == 0.0);
    menu_item_add_child(parent, new_item);
    
    gtk_entry_set_text(GTK_ENTRY(app->name_entry), "");
    gtk_entry_set_text(GTK_ENTRY(app->price_entry), "");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->parent_combo), 0);
    
    gtk_label_set_text(GTK_LABEL(app->info_label), "Элемент добавлен!");
    
    refresh_tree_view(app);
    update_parent_combo(app);
    
    g_free(parent_path);
}

static void free_items_recursive(MenuItem *item) {
    for (int i = 0; i < item->child_count; i++) {
        free_items_recursive(item->children[i]);
    }
    g_free(item->children);
    g_free(item);
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget;
    (void)event;
    AppData *app = (AppData *)user_data;
    
    free_items_recursive(app->root);
    g_object_unref(app->css_provider);
    g_free(app);
    
    return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;
    AppData *data = g_new0(AppData, 1);
    data->root = menu_item_create("Меню", 0.0, TRUE);
    load_default_menu(data->root);
    
    GtkWidget *window = gtk_application_window_new(app);
    data->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Кафе - Иерархическое меню");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    
    load_css(data);
    
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);
    
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "header");
    gtk_box_pack_start(GTK_BOX(main_vbox), header, FALSE, FALSE, 0);
    
    GtkWidget *title = gtk_label_new("Иерархическое меню напитков");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    
    GtkWidget *fullscreen_button = gtk_toggle_button_new_with_label("Полный экран");
    gtk_box_pack_end(GTK_BOX(header), fullscreen_button, FALSE, FALSE, 0);
    g_signal_connect(fullscreen_button, "toggled", G_CALLBACK(gtk_window_fullscreen), window);
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(main_vbox), scroll, TRUE, TRUE, 0);
    
    data->tree_view = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->tree_view), TRUE);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Название", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "Цена", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(data->tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", 
                     G_CALLBACK(on_tree_selection_changed), data);
    g_signal_connect(data->tree_view, "row-activated",
                     G_CALLBACK(on_row_activated), data);
    
    gtk_container_add(GTK_CONTAINER(scroll), data->tree_view);
    refresh_tree_view(data);
    
    GtkWidget *add_frame = gtk_frame_new("Добавить элемент");
    gtk_box_pack_start(GTK_BOX(main_vbox), add_frame, FALSE, FALSE, 0);
    
    GtkWidget *add_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(add_frame), add_box);
    gtk_widget_set_margin_top(add_box, 10);
    gtk_widget_set_margin_bottom(add_box, 10);
    gtk_widget_set_margin_start(add_box, 10);
    gtk_widget_set_margin_end(add_box, 10);
    
    data->name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->name_entry), "Название");
    gtk_box_pack_start(GTK_BOX(add_box), data->name_entry, TRUE, TRUE, 0);
    
    data->price_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->price_entry), "Цена (руб., опционально)");
    gtk_box_pack_start(GTK_BOX(add_box), data->price_entry, TRUE, TRUE, 0);
    
    data->parent_combo = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(add_box), data->parent_combo, TRUE, TRUE, 0);
    
    GtkWidget *add_button = gtk_button_new_with_label("Добавить");
    gtk_box_pack_start(GTK_BOX(add_box), add_button, FALSE, FALSE, 0);
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_item_clicked), data);
    
    data->info_label = gtk_label_new("Выберите элемент из меню");
    gtk_style_context_add_class(gtk_widget_get_style_context(data->info_label), "info-label");
    gtk_box_pack_start(GTK_BOX(main_vbox), data->info_label, FALSE, FALSE, 0);
    
    g_signal_connect(window, "delete-event", G_CALLBACK(on_window_delete), data);
    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.example.drinkmenu", 
                                               G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}