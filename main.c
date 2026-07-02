#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ITEMS 500
#define MAX_NAME_LEN 64
#define MAX_PATH_LEN 256
#define MAX_CART_ITEMS 50
#define MAX_DEPTH 7

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
    MenuItem *menu_item;
    int quantity;
} CartItem;

typedef struct {
    CartItem items[MAX_CART_ITEMS];
    int count;
} Cart;

typedef struct {
    MenuItem *root;
    MenuItem *selected_item;
    MenuItem *context_item;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *menu_tree_view;
    GtkWidget *cart_tree_view;
    GtkWidget *info_label;
    GtkWidget *name_entry;
    GtkWidget *price_entry;
    GtkWidget *parent_combo;
    GtkWidget *total_label;
    GtkCssProvider *css_provider;
    Cart *cart;
    int edit_mode;
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

static int get_depth(MenuItem *item) {
    int depth = 0;
    while (item->parent != NULL) {
        depth++;
        item = item->parent;
    }
    return depth;
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

static Cart* cart_create() {
    Cart *cart = g_new0(Cart, 1);
    return cart;
}

static void cart_add_item(Cart *cart, MenuItem *menu_item) {
    if (!menu_item || menu_item->is_category) return;
    
    for (int i = 0; i < cart->count; i++) {
        if (cart->items[i].menu_item == menu_item) {
            cart->items[i].quantity++;
            return;
        }
    }
    
    if (cart->count >= MAX_CART_ITEMS) return;
    
    cart->items[cart->count].menu_item = menu_item;
    cart->items[cart->count].quantity = 1;
    cart->count++;
}

static void cart_remove_item(Cart *cart, int index) {
    if (index < 0 || index >= cart->count) return;
    
    for (int i = index; i < cart->count - 1; i++) {
        cart->items[i] = cart->items[i + 1];
    }
    cart->count--;
}

static double cart_get_total(Cart *cart) {
    double total = 0.0;
    for (int i = 0; i < cart->count; i++) {
        total += cart->items[i].menu_item->price * cart->items[i].quantity;
    }
    return total;
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
            snprintf(info, sizeof(info), "Категория: %s\nНажмите + для добавления подкатегории", name);
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
    AppData *app = (AppData *)user_data;
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
        } else {
            MenuItem *item = find_item_by_path(app->root, path_str);
            if (item) {
                cart_add_item(app->cart, item);
                char info[128];
                snprintf(info, sizeof(info), "Добавлено: %s (%.2f руб.)", item->name, item->price);
                gtk_label_set_text(GTK_LABEL(app->info_label), info);
            }
        }
        
        g_free(name);
        g_free(path_str);
    }
}

static void on_add_to_cart_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *app = (AppData *)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->menu_tree_view));
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
        
        if (!is_category) {
            MenuItem *item = find_item_by_path(app->root, path_str);
            if (item) {
                cart_add_item(app->cart, item);
                char info[128];
                snprintf(info, sizeof(info), "Добавлено: %s (%.2f руб.)", item->name, item->price);
                gtk_label_set_text(GTK_LABEL(app->info_label), info);
            }
        } else {
            gtk_label_set_text(GTK_LABEL(app->info_label), "Выберите товар, а не категорию!");
        }
        
        g_free(name);
        g_free(path_str);
    }
}

static void on_remove_from_cart_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *app = (AppData *)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->cart_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        int index;
        gtk_tree_model_get(model, &iter, 2, &index, -1);
        cart_remove_item(app->cart, index);
        
        double total = cart_get_total(app->cart);
        char total_str[64];
        snprintf(total_str, sizeof(total_str), "Итого: %.2f руб.", total);
        gtk_label_set_text(GTK_LABEL(app->total_label), total_str);
        
        gtk_tree_view_set_model(GTK_TREE_VIEW(app->cart_tree_view), NULL);
        GtkListStore *list_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_DOUBLE);
        GtkTreeIter list_iter;
        
        for (int i = 0; i < app->cart->count; i++) {
            gtk_list_store_append(list_store, &list_iter);
            char quantity_str[32];
            snprintf(quantity_str, sizeof(quantity_str), "%d шт.", app->cart->items[i].quantity);
            gtk_list_store_set(list_store, &list_iter,
                               0, app->cart->items[i].menu_item->name,
                               1, quantity_str,
                               2, i,
                               3, app->cart->items[i].menu_item->price * app->cart->items[i].quantity,
                               -1);
        }
        
        gtk_tree_view_set_model(GTK_TREE_VIEW(app->cart_tree_view), GTK_TREE_MODEL(list_store));
        g_object_unref(list_store);
    }
}

static void on_checkout_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *app = (AppData *)user_data;
    
    if (app->cart->count == 0) {
        gtk_label_set_text(GTK_LABEL(app->info_label), "Корзина пуста!");
        return;
    }
    
    double total = cart_get_total(app->cart);
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK_CANCEL,
                                               "Оплата\nСумма к оплате: %.2f руб.\n\nОтправить на терминал?", total);
    gtk_window_set_title(GTK_WINDOW(dialog), "Оплата");
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        gchar *command = g_strdup_printf("echo '%.2f' | xargs -0 printf 'Сумма: %%s руб.\\n'", total);
        int result = system(command);
        g_free(command);
        
        if (result == 0) {
            gtk_label_set_text(GTK_LABEL(app->info_label), "Заказ отправлен на оплату!");
            
            app->cart->count = 0;
            gtk_label_set_text(GTK_LABEL(app->total_label), "Итого: 0.00 руб.");
            gtk_tree_view_set_model(GTK_TREE_VIEW(app->cart_tree_view), NULL);
        } else {
            gtk_label_set_text(GTK_LABEL(app->info_label), "Ошибка отправки на терминал!");
        }
    }
    
    gtk_widget_destroy(dialog);
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
    gtk_tree_view_set_model(GTK_TREE_VIEW(app->menu_tree_view), GTK_TREE_MODEL(store));
    g_object_unref(store);
}

static void add_items_recursive(GtkListStore *list_store, GtkTreeIter *parent_iter,
                                MenuItem *item, const char *path) {
    (void)parent_iter;
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
    
    int depth = get_depth(parent);
    if (depth >= MAX_DEPTH - 1) {
        gtk_label_set_text(GTK_LABEL(app->info_label), "Ошибка: максимальная глубина вложенности!");
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

static void on_add_category_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AppData *app = (AppData *)user_data;
    
    if (!app->context_item || !app->context_item->is_category) {
        gtk_label_set_text(GTK_LABEL(app->info_label), "Выберите категорию для добавления подкатегории!");
        return;
    }
    
    int depth = get_depth(app->context_item);
    if (depth >= MAX_DEPTH - 1) {
        gtk_label_set_text(GTK_LABEL(app->info_label), "Ошибка: максимальная глубина вложенности (7 уровней)!");
        return;
    }
    
    gtk_entry_set_text(GTK_ENTRY(app->name_entry), "");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->name_entry), "Новая категория (без цены)");
    gtk_entry_set_text(GTK_ENTRY(app->price_entry), "0");
    
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s", app->context_item->name);
    MenuItem *current = app->context_item;
    while (current->parent != NULL && current->parent != app->root) {
        char temp[MAX_PATH_LEN];
        snprintf(temp, sizeof(temp), "%s/%s", current->parent->name, path);
        strncpy(path, temp, sizeof(path));
        current = current->parent;
    }
    
    gtk_label_set_text(GTK_LABEL(app->info_label), "Введите название новой категории и нажмите 'Добавить'");
    
    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), 1);
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
    g_free(app->cart);
    g_object_unref(app->css_provider);
    g_free(app);
    
    return FALSE;
}

static void on_context_menu(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    
    GtkTreePath *path;
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(app->menu_tree_view),
                                      event->button.x, event->button.y,
                                      &path, NULL, NULL, NULL)) {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(app->menu_tree_view));
        GtkTreeIter iter;
        
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gchar *path_str;
            gtk_tree_model_get(model, &iter, 3, &path_str, -1);
            
            app->context_item = find_item_by_path(app->root, path_str);
            g_free(path_str);
            
            gtk_tree_path_free(path);
            
            on_add_category_clicked(NULL, app);
        }
    }
}

static gboolean on_tree_view_button_press(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button.button == 3) {
        on_context_menu(widget, event, user_data);
        return TRUE;
    }
    return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;
    AppData *data = g_new0(AppData, 1);
    data->root = menu_item_create("Меню", 0.0, TRUE);
    data->cart = cart_create();
    data->edit_mode = FALSE;
    load_default_menu(data->root);
    
    GtkWidget *window = gtk_application_window_new(app);
    data->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Кафе - Система управления");
    gtk_window_set_default_size(GTK_WINDOW(window), 1600, 1200);
    
    load_css(data);
    
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);
    
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "header");
    gtk_box_pack_start(GTK_BOX(main_vbox), header, FALSE, FALSE, 0);
    
    GtkWidget *title = gtk_label_new("Система управления кафе");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    
    GtkWidget *edit_button = gtk_toggle_button_new_with_label("Редактировать меню");
    gtk_box_pack_start(GTK_BOX(header), edit_button, FALSE, FALSE, 0);
    g_signal_connect(edit_button, "toggled", G_CALLBACK(gtk_toggle_button_get_active), data);
    
    GtkWidget *fullscreen_button = gtk_toggle_button_new_with_label("Полный экран");
    gtk_box_pack_end(GTK_BOX(header), fullscreen_button, FALSE, FALSE, 0);
    g_signal_connect(fullscreen_button, "toggled", G_CALLBACK(gtk_window_fullscreen), window);
    
    data->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(main_vbox), data->notebook, TRUE, TRUE, 0);
    
    GtkWidget *menu_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(menu_tab), 15);
    
    GtkWidget *menu_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(menu_scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(menu_tab), menu_scroll, TRUE, TRUE, 0);
    
    data->menu_tree_view = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->menu_tree_view), TRUE);
    gtk_widget_set_size_request(data->menu_tree_view, 800, 600);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Название", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->menu_tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "Цена (руб.)", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->menu_tree_view), column);
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(data->menu_tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", 
                     G_CALLBACK(on_tree_selection_changed), data);
    g_signal_connect(data->menu_tree_view, "row-activated",
                     G_CALLBACK(on_row_activated), data);
    g_signal_connect(data->menu_tree_view, "button-press-event",
                     G_CALLBACK(on_tree_view_button_press), data);
    
    gtk_container_add(GTK_CONTAINER(menu_scroll), data->menu_tree_view);
    refresh_tree_view(data);
    
    GtkWidget *add_to_cart_button = gtk_button_new_with_label("Добавить в корзину");
    gtk_style_context_add_class(gtk_widget_get_style_context(add_to_cart_button), "add-to-cart-button");
    gtk_box_pack_start(GTK_BOX(menu_tab), add_to_cart_button, FALSE, FALSE, 0);
    g_signal_connect(add_to_cart_button, "clicked", G_CALLBACK(on_add_to_cart_clicked), data);
    
    GtkWidget *cart_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(cart_tab), 15);
    
    GtkWidget *cart_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cart_scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(cart_tab), cart_scroll, TRUE, TRUE, 0);
    
    data->cart_tree_view = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->cart_tree_view), TRUE);
    gtk_widget_set_size_request(data->cart_tree_view, 800, 600);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "Название", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->cart_tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "Кол-во", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->cart_tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "Сумма", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->cart_tree_view), column);
    
    gtk_container_add(GTK_CONTAINER(cart_scroll), data->cart_tree_view);
    
    GtkWidget *cart_buttons_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_pack_start(GTK_BOX(cart_tab), cart_buttons_hbox, FALSE, FALSE, 0);
    
    GtkWidget *remove_button = gtk_button_new_with_label("Удалить выбранное");
    gtk_style_context_add_class(gtk_widget_get_style_context(remove_button), "remove-button");
    gtk_box_pack_start(GTK_BOX(cart_buttons_hbox), remove_button, TRUE, TRUE, 0);
    g_signal_connect(remove_button, "clicked", G_CALLBACK(on_remove_from_cart_clicked), data);
    
    data->total_label = gtk_label_new("Итого: 0.00 руб.");
    gtk_style_context_add_class(gtk_widget_get_style_context(data->total_label), "total-label");
    gtk_box_pack_end(GTK_BOX(cart_tab), data->total_label, FALSE, FALSE, 0);
    
    GtkWidget *checkout_button = gtk_button_new_with_label("Оплатить");
    gtk_style_context_add_class(gtk_widget_get_style_context(checkout_button), "checkout-button");
    gtk_box_pack_start(GTK_BOX(cart_tab), checkout_button, FALSE, FALSE, 0);
    g_signal_connect(checkout_button, "clicked", G_CALLBACK(on_checkout_clicked), data);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(data->notebook), menu_tab, gtk_label_new("Меню"));
    gtk_notebook_append_page(GTK_NOTEBOOK(data->notebook), cart_tab, gtk_label_new("Корзина"));
    
    GtkWidget *edit_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(edit_tab), 15);
    
    GtkWidget *add_frame = gtk_frame_new("Добавить / Редактировать элемент");
    gtk_style_context_add_class(gtk_widget_get_style_context(add_frame), "add-frame");
    gtk_box_pack_start(GTK_BOX(edit_tab), add_frame, FALSE, FALSE, 0);
    
    GtkWidget *add_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_container_add(GTK_CONTAINER(add_frame), add_box);
    gtk_widget_set_margin_top(add_box, 15);
    gtk_widget_set_margin_bottom(add_box, 15);
    gtk_widget_set_margin_start(add_box, 15);
    gtk_widget_set_margin_end(add_box, 15);
    
    data->name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->name_entry), "Название");
    gtk_box_pack_start(GTK_BOX(add_box), data->name_entry, TRUE, TRUE, 0);
    
    data->price_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->price_entry), "Цена (руб., пусто = категория)");
    gtk_box_pack_start(GTK_BOX(add_box), data->price_entry, TRUE, TRUE, 0);
    
    data->parent_combo = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(add_box), data->parent_combo, TRUE, TRUE, 0);
    
    GtkWidget *add_button = gtk_button_new_with_label("Добавить");
    gtk_style_context_add_class(gtk_widget_get_style_context(add_button), "add-to-cart-button");
    gtk_box_pack_start(GTK_BOX(add_box), add_button, FALSE, FALSE, 0);
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_item_clicked), data);
    
    data->info_label = gtk_label_new("Правый клик на категории для добавления подкатегории");
    gtk_style_context_add_class(gtk_widget_get_style_context(data->info_label), "info-label");
    gtk_box_pack_start(GTK_BOX(edit_tab), data->info_label, FALSE, FALSE, 0);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(data->notebook), edit_tab, gtk_label_new("Редактирование"));
    
    update_parent_combo(data);
    
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