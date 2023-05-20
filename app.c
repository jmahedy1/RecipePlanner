//  Compile: gcc -o app app.c -lcurl -lcjson `pkg-config --cflags --libs gtk+-3.0`
//  Run: ./app

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <gtk/gtk.h>

// global variables with all the UI elements
GtkWidget *window;
GtkWidget *grid;
GtkWidget *field_label;
GtkWidget *text_field;
GtkWidget *diet_label;
GtkWidget *combo_box;
GtkWidget *button;
GtkWidget *scrolled_window; // add scrolled_window widget
GtkWidget *text_area;
GtkTextBuffer *text_buffer;
gchar *input;
char *ingredients;
char *diet;

//struct to store the response data and its length
struct curl_response
{
    char *data;
    size_t len;
};

//function will be called by cURL when data is received
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    struct curl_response *response = (struct curl_response *)userdata; //cast userdata back to curl_response struct.
    size_t realsize = size * nmemb; //calculate the total size of the data received.
    response->data = realloc(response->data, response->len + realsize + 1);//reallocate memory and append new data.
    if (response->data == NULL) //check if reallocation was successful.
    {
        fprintf(stderr, "write_callback(): realloc() failed\n");
        return 0;
    }

    //copy the new data to end of existing response data.
    memcpy(&(response->data[response->len]), ptr, realsize);
    response->len += realsize;

    //add null terminator at end of response data.
    response->data[response->len] = '\0';
    return realsize;    //return size of data received.
}

char* remove_spaces(char* strin)
{
    int i,j;
    char *strout=strin;
    for (i = 0, j = 0; i<strlen(strin); i++,j++)
    {
        if (strin[i]!=' ')
            strout[j]=strin[i];
        else
            j--;
    }
    strout[j]=0;
    return strout;
}

void on_button_clicked(GtkButton *button, gpointer data) {
    CURL *curl_handle;  //pointer to a cURL handle
    CURLcode res;       //variable to store result of cURL operation
    struct curl_response response = {0};  //curl_response struct to store response data
    char url[1024];     //character array to store URL of API request

    GtkTextView *text_view = GTK_TEXT_VIEW(data);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

    input = gtk_entry_get_text(text_field);
    ingredients = remove_spaces(input);

    gint active_item = gtk_combo_box_get_active(combo_box);
    if(active_item == 0)
    {
        diet = "";
    }
    else if (active_item ==1)
    {
        diet = "Vegetarian";
    }
    else if(active_item == 2)
    {
        diet = "Gluten Free";
    }
    else if(active_item == 3)
    {
        diet = "Vegan";
    }
    else if(active_item == 4)
    {
        diet = "Pescetarian";
    }

    printf("\n%s", ingredients);
    printf("\n%s\n", diet);

    curl_global_init(CURL_GLOBAL_ALL);  //initialize cURL
    curl_handle = curl_easy_init(); //create cURL handle

    if(!curl_handle)   //handle errors in cURL handle creation
    {
        fprintf(stderr, "curl_easy_init() failed\n");
        return;
    }

    //create the URL for the API request using the given ingredients and dietary restriction
    snprintf(url, sizeof(url), "https://api.spoonacular.com/recipes/findByIngredients"
                               "?apiKey=e86a2c9a1318492ea3faaea8ce9d58b3"
                               "&ingredients=%s"
                               "&diet=%s"
                               "&number=1", ingredients, diet);

    //set URL and callback function for cURL to perform API request and store response
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response);

    //perform API request and handle errors
    res = curl_easy_perform(curl_handle); //send HTTP request and get response
    if(res != CURLE_OK) //check if request was successful
    {
        gtk_text_buffer_insert_at_cursor(buffer, "No recipe found!\nModify ingredients or dietary restriction!\n\n", -1);
        curl_easy_cleanup(curl_handle); //clean up cURL handle
        curl_global_cleanup(); //clean up global resources
        return; //exit the function
    }

    //parse JSON response using cJSON library
    cJSON *root = cJSON_Parse(response.data); //parse JSON data
    if(!root) //check if parsing was successful
    {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr()); // Print an error message
        free(response.data); //free memory allocated for response data
        curl_easy_cleanup(curl_handle); //clean up cURL handle
        curl_global_cleanup(); //clean up global resources
        return; //exit the function
    }

    //extract and print title
    cJSON *recipe = cJSON_GetArrayItem(root, 0); //get first recipe from root JSON object
    cJSON *title = cJSON_GetObjectItem(recipe, "title"); //get value of title property from recipe object
    gtk_text_buffer_insert_at_cursor(buffer, "Title: ", -1);
    gtk_text_buffer_insert_at_cursor(buffer, title->valuestring, -1);
    gtk_text_buffer_insert_at_cursor(buffer, "\n\n", -1);

    //print used ingredients
    gtk_text_buffer_insert_at_cursor(buffer, "Ingredients:", -1);
    gtk_text_buffer_insert_at_cursor(buffer, "\n", -1);
    cJSON *usedIngredients = cJSON_GetObjectItem(recipe, "usedIngredients"); //get value of usedIngredients object
    for(int i = 0; i < cJSON_GetArraySize(usedIngredients); i++)    //loop through each used ingredient in array
    {
        cJSON *usedIngredient = cJSON_GetArrayItem(usedIngredients, i); //get current ingredient object
        cJSON *usedName = cJSON_GetObjectItem(usedIngredient, "original"); //get value of original property from object
        gtk_text_buffer_insert_at_cursor(buffer, usedName->valuestring, -1);
        gtk_text_buffer_insert_at_cursor(buffer, "\n", -1);
    }

    //print missing ingredients
    cJSON *missedIngredients = cJSON_GetObjectItem(recipe, "missedIngredients"); //get value of missedIngredients object
    for(int i = 0; i < cJSON_GetArraySize(missedIngredients); i++)  //loop through each missing ingredient in array
    {
        cJSON *missingIngredient = cJSON_GetArrayItem(missedIngredients, i); //get current missing ingredient object
        cJSON *missingName = cJSON_GetObjectItem(missingIngredient, "original"); //get value of original property object
        gtk_text_buffer_insert_at_cursor(buffer, missingName->valuestring, -1);
        gtk_text_buffer_insert_at_cursor(buffer, "\n", -1);

    }
    gtk_text_buffer_insert_at_cursor(buffer, "\n", -1);
    gtk_text_buffer_insert_at_cursor(buffer, "\n", -1);

    cJSON_Delete(root); //delete cJSON root object
    free(response.data); //free memory allocated for response data
    curl_easy_cleanup(curl_handle); //clean up cURL handle
    curl_global_cleanup(); //clean up global resources
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Meal Planner");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 500);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create grid layout
    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    // create text label
    field_label = gtk_label_new("Type the ingredients you have access to below separated by commas: ");
    gtk_grid_attach(GTK_GRID(grid), field_label, 0, 0, 2, 1);

    // Create text field
    text_field = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), text_field, 0, 1, 2, 1);
    gtk_widget_set_hexpand(text_field, TRUE);
    gtk_widget_set_halign(text_field, GTK_ALIGN_FILL);

    // create diet label
    diet_label = gtk_label_new("Select your dietary restrictions below: ");
    gtk_grid_attach(GTK_GRID(grid), diet_label, 0, 2, 2, 1);

    // Create dropdown menu
    combo_box = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "None");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Vegetarian");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Gluten-free");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Vegan");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), "Pescatarian");
    gtk_grid_attach(GTK_GRID(grid), combo_box, 0, 3, 2, 1);
    gtk_widget_set_hexpand(combo_box, TRUE);
    gtk_widget_set_halign(combo_box, GTK_ALIGN_FILL);

    // Create button
    button = gtk_button_new_with_label("Find Recipes!");
    gtk_grid_attach(GTK_GRID(grid), button, 0, 5, 2, 1);
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);

    // Create scrolled window
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 6, 2, 5);
    gtk_widget_set_size_request(scrolled_window, -1, 300); // set the height of the scrolled window

    // Create text view
    text_area = gtk_text_view_new();
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_area);
    gtk_widget_set_hexpand(text_area, TRUE);
    gtk_widget_set_halign(text_area, GTK_ALIGN_FILL);

    // Connect button signal
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), text_area);

    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}