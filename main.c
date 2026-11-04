
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CARS 500
#define MAX_CUSTOMERS 500
#define MAX_SALES 1000
#define TEXT 64
#define DB_DIR "data"
#define CAR_FILE DB_DIR "/cars.db"
#define CUSTOMER_FILE DB_DIR "/customers.db"
#define SALE_FILE DB_DIR "/sales.db"

typedef struct {
    int id, year, mileage;
    char make[TEXT], model[TEXT];
    char fuel[TEXT], transmission[TEXT], colour[TEXT];
    double price;
    int available;
} Car;

typedef struct {
    int id;
    char name[TEXT], email[TEXT], phone[TEXT];
} Customer;

typedef struct {
    int id, car_id, customer_id;
    double sale_price;
    char date[32];
} Sale;

typedef struct {
    Car cars[MAX_CARS];
    size_t car_count;

    Customer customers[MAX_CUSTOMERS];
    size_t customer_count;

    Sale sales[MAX_SALES];
    size_t sale_count;

    int next_car_id;
    int next_customer_id;
    int next_sale_id;
} Dealership;

static void line(void) {
    puts("--------------------------------------------------------------------------");
}

static void trim(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

static int read_text(const char *prompt, char *out, size_t cap) {
    printf("%s", prompt);

    if (!fgets(out, (int)cap, stdin)) return 0;

    if (!strchr(out, '\n')) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
    }

    trim(out);

    for (size_t i = 0; out[i]; i++) {
        if (out[i] == '|') out[i] = '/';
    }

    return 1;
}

static int read_int(const char *prompt, int *out) {
    char b[128], *end;
    long v;

    if (!read_text(prompt, b, sizeof b)) return 0;

    errno = 0;
    v = strtol(b, &end, 10);

    while (isspace((unsigned char)*end)) end++;

    if (b[0] == '\0' || *end || errno ||
        v < -2147483647L - 1 || v > 2147483647L) {
        puts("Invalid whole number.");
        return -1;
    }

    *out = (int)v;
    return 1;
}

static int read_money(const char *prompt, double *out) {
    char b[128], *end;
    double v;

    if (!read_text(prompt, b, sizeof b)) return 0;

    errno = 0;
    v = strtod(b, &end);

    while (isspace((unsigned char)*end)) end++;

    if (b[0] == '\0' || *end || errno || v <= 0.0) {
        puts("Enter an amount greater than zero.");
        return -1;
    }

    *out = v;
    return 1;
}

static void seed(Dealership *d) {
    const Car demo[] = {
        {1,2023,12000,"Toyota","GR Supra GTS","Petrol","Automatic","White",92990,1},
        {2,2022,18500,"BMW","M3 Competition","Petrol","Automatic","Black",139900,1},
        {3,2024,4500,"Mazda","MX-5 RF GT","Petrol","Manual","Soul Red",52990,1},
        {4,2021,22000,"Porsche","911 Carrera","Petrol","PDK","Silver",239000,1},
        {5,2023,9000,"Ford","Mustang GT","Petrol","Manual","Blue",82990,1},
        {6,2022,15000,"Hyundai","i30 N Premium","Petrol","Manual","Performance Blue",47990,1},
        {7,2024,7000,"Tesla","Model 3 Long Range","Electric","Automatic","Pearl White",68900,1}
    };

    memcpy(d->cars, demo, sizeof demo);
    d->car_count = sizeof demo / sizeof demo[0];

    d->next_car_id = 8;
    d->next_customer_id = 1;
    d->next_sale_id = 1;
}

static void save_all(const Dealership *d) {
    FILE *f;
    size_t i;

    (void)system("mkdir -p " DB_DIR);

    f = fopen(CAR_FILE, "w");
    if (!f) {
        perror("Saving cars");
        return;
    }

    for (i = 0; i < d->car_count; i++) {
        const Car *c = &d->cars[i];

        fprintf(f,
            "%d|%d|%d|%s|%s|%s|%s|%s|%.2f|%d\n",
            c->id, c->year, c->mileage,
            c->make, c->model, c->fuel,
            c->transmission, c->colour,
            c->price, c->available
        );
    }

    fclose(f);

    f = fopen(CUSTOMER_FILE, "w");
    if (!f) {
        perror("Saving customers");
        return;
    }

    for (i = 0; i < d->customer_count; i++) {
        const Customer *c = &d->customers[i];

        fprintf(f, "%d|%s|%s|%s\n",
            c->id, c->name, c->email, c->phone);
    }

    fclose(f);

    f = fopen(SALE_FILE, "w");
    if (!f) {
        perror("Saving sales");
        return;
    }

    for (i = 0; i < d->sale_count; i++) {
        const Sale *s = &d->sales[i];

        fprintf(f, "%d|%d|%d|%.2f|%s\n",
            s->id, s->car_id, s->customer_id,
            s->sale_price, s->date);
    }

    fclose(f);
}

static void load_all(Dealership *d) {
    FILE *f;
    char b[512];

    f = fopen(CAR_FILE, "r");

    if (!f) {
        seed(d);
        save_all(d);
        return;
    }

    while (d->car_count < MAX_CARS && fgets(b, sizeof b, f)) {
        Car c;

        if (sscanf(b,
            "%d|%d|%d|%63[^|]|%63[^|]|%63[^|]|%63[^|]|%63[^|]|%lf|%d",
            &c.id, &c.year, &c.mileage,
            c.make, c.model, c.fuel,
            c.transmission, c.colour,
            &c.price, &c.available) == 10) {
            d->cars[d->car_count++] = c;
        }
    }

    fclose(f);

    f = fopen(CUSTOMER_FILE, "r");

    if (f) {
        while (d->customer_count < MAX_CUSTOMERS &&
               fgets(b, sizeof b, f)) {
            Customer c;

            if (sscanf(b, "%d|%63[^|]|%63[^|]|%63[^|]",
                &c.id, c.name, c.email, c.phone) == 4) {
                d->customers[d->customer_count++] = c;
            }
        }

        fclose(f);
    }

    f = fopen(SALE_FILE, "r");

    if (f) {
        while (d->sale_count < MAX_SALES && fgets(b, sizeof b, f)) {
            Sale s;

            if (sscanf(b, "%d|%d|%d|%lf|%31[^\n]",
                &s.id, &s.car_id, &s.customer_id,
                &s.sale_price, s.date) == 5) {
                d->sales[d->sale_count++] = s;
            }
        }

        fclose(f);
    }

    d->next_car_id = 1;
    d->next_customer_id = 1;
    d->next_sale_id = 1;

    for (size_t i = 0; i < d->car_count; i++)
        if (d->cars[i].id >= d->next_car_id)
            d->next_car_id = d->cars[i].id + 1;

    for (size_t i = 0; i < d->customer_count; i++)
        if (d->customers[i].id >= d->next_customer_id)
            d->next_customer_id = d->customers[i].id + 1;

    for (size_t i = 0; i < d->sale_count; i++)
        if (d->sales[i].id >= d->next_sale_id)
            d->next_sale_id = d->sales[i].id + 1;
}

static Car *find_car(Dealership *d, int id) {
    for (size_t i = 0; i < d->car_count; i++)
        if (d->cars[i].id == id)
            return &d->cars[i];

    return NULL;
}

static void print_car(const Car *c) {
    printf("#%-4d %-12s %-20s %4d %7d km %-9s %-10s $%10.2f %s\n",
        c->id, c->make, c->model, c->year,
        c->mileage, c->fuel, c->transmission,
        c->price, c->available ? "IN STOCK" : "SOLD");
}

static void list_cars(const Dealership *d, int available_only) {
    int n = 0;

    puts("\nID    MAKE         MODEL                 YEAR  ODOMETER  FUEL      GEARBOX    PRICE       STATUS");
    line();

    for (size_t i = 0; i < d->car_count; i++) {
        if (!available_only || d->cars[i].available) {
            print_car(&d->cars[i]);
            n++;
        }
    }

    if (!n) puts("No vehicles match.");

    printf("\n%d vehicle(s) displayed.\n", n);
}

static int contains_ci(const char *hay, const char *needle) {
    char a[TEXT * 2], b[TEXT];

    snprintf(a, sizeof a, "%s", hay);
    snprintf(b, sizeof b, "%s", needle);

    for (size_t i = 0; a[i]; i++)
        a[i] = (char)tolower((unsigned char)a[i]);

    for (size_t i = 0; b[i]; i++)
        b[i] = (char)tolower((unsigned char)b[i]);

    return strstr(a, b) != NULL;
}

static void search_cars(const Dealership *d) {
    char q[TEXT];
    int found = 0;

    if (!read_text("Search make/model/fuel: ", q, sizeof q) || !q[0])
        return;

    puts("\nSearch results:");

    for (size_t i = 0; i < d->car_count; i++) {
        const Car *c = &d->cars[i];

        if (c->available &&
            (contains_ci(c->make, q) ||
             contains_ci(c->model, q) ||
             contains_ci(c->fuel, q))) {
            print_car(c);
            found++;
        }
    }

    if (!found) puts("No matching vehicles found.");
}

static void sort_cars(Dealership *d) {
    int choice;

    if (read_int("Sort by: 1 price low-high, 2 price high-low, 3 year new-old: ",
        &choice) != 1)
        return;

    for (size_t i = 0; i < d->car_count; i++) {
        for (size_t j = i + 1; j < d->car_count; j++) {
            int swap = 0;
            Car *a = &d->cars[i];
            Car *b = &d->cars[j];

            if (choice == 1 && a->price > b->price) swap = 1;
            else if (choice == 2 && a->price < b->price) swap = 1;
            else if (choice == 3 && a->year < b->year) swap = 1;

            if (swap) {
                Car t = *a;
                *a = *b;
                *b = t;
            }
        }
    }

    puts("Inventory sorted.");
    list_cars(d, 1);
}

static void add_car(Dealership *d) {
    Car c = {0};
    int r;

    if (d->car_count >= MAX_CARS) {
        puts("Inventory capacity reached.");
        return;
    }

    c.id = d->next_car_id;

    if (!read_text("Make: ", c.make, sizeof c.make) || !c.make[0] ||
        !read_text("Model: ", c.model, sizeof c.model) || !c.model[0])
        return;

    if ((r = read_int("Year: ", &c.year)) != 1 ||
        c.year < 1886 || c.year > 2100) {
        puts("Invalid year.");
        return;
    }

    if ((r = read_int("Odometer (km): ", &c.mileage)) != 1 ||
        c.mileage < 0) {
        puts("Invalid odometer.");
        return;
    }

    if (!read_text("Fuel type: ", c.fuel, sizeof c.fuel) || !c.fuel[0] ||
        !read_text("Transmission: ", c.transmission, sizeof c.transmission) ||
        !c.transmission[0] ||
        !read_text("Colour: ", c.colour, sizeof c.colour) || !c.colour[0])
        return;

    if (read_money("Asking price (AUD): $", &c.price) != 1)
        return;

    c.available = 1;
    d->cars[d->car_count++] = c;
    d->next_car_id++;

    save_all(d);

    printf("Added %s %s with stock ID %d.\n", c.make, c.model, c.id);
}

static void update_price(Dealership *d) {
    int id;
    double p;

    if (read_int("Vehicle ID: ", &id) != 1) return;

    Car *c = find_car(d, id);

    if (!c || !c->available) {
        puts("Available vehicle not found.");
        return;
    }

    if (read_money("New asking price: $", &p) != 1) return;

    c->price = p;
    save_all(d);
    puts("Price updated.");
}

static void remove_car(Dealership *d) {
    int id;

    if (read_int("Vehicle ID to remove: ", &id) != 1) return;

    Car *c = find_car(d, id);

    if (!c || !c->available) {
        puts("Available vehicle not found.");
        return;
    }

    c->available = 0;
    save_all(d);

    puts("Vehicle archived from active stock.");
}

static void add_customer(Dealership *d, Customer *c) {
    c->id = d->next_customer_id++;
    d->customers[d->customer_count++] = *c;
}

static void purchase(Dealership *d) {
    int id;
    char confirm[8];

    if (d->sale_count >= MAX_SALES ||
        d->customer_count >= MAX_CUSTOMERS) {
        puts("Sales/customer capacity reached.");
        return;
    }

    if (read_int("Vehicle ID to purchase: ", &id) != 1) return;

    Car *c = find_car(d, id);

    if (!c || !c->available) {
        puts("Vehicle unavailable.");
        return;
    }

    printf("\nSelected: %d %s %s (%d)\nPrice: AUD $%.2f\n",
        c->id, c->make, c->model, c->year, c->price);

    Customer customer = {0};

    if (!read_text("Customer full name: ", customer.name,
        sizeof customer.name) || !customer.name[0] ||
        !read_text("Email: ", customer.email, sizeof customer.email) ||
        !read_text("Phone: ", customer.phone, sizeof customer.phone))
        return;

    if (!read_text("Confirm sale? (y/n): ", confirm, sizeof confirm) ||
        tolower((unsigned char)confirm[0]) != 'y' ||
        confirm[1] != '\0') {
        puts("Sale cancelled.");
        return;
    }

    add_customer(d, &customer);

    Sale s = {0};
    s.id = d->next_sale_id++;
    s.car_id = c->id;
    s.customer_id = customer.id;
    s.sale_price = c->price;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(s.date, sizeof s.date, "%Y-%m-%d %H:%M", tm_info);

    d->sales[d->sale_count++] = s;
    c->available = 0;

    save_all(d);

    line();
    puts("                     AUTOLOT SALES RECEIPT");
    line();

    printf("Receipt: #%d\nDate: %s\nCustomer: %s\nVehicle: %s %s (%d)\nSale price: AUD $%.2f\n",
        s.id, s.date, customer.name,
        c->make, c->model, c->year, s.sale_price);

    line();
    puts("Sale recorded successfully.");
}

static void sales_history(const Dealership *d) {
    puts("\nSALE HISTORY");
    line();

    if (!d->sale_count) {
        puts("No completed sales yet.");
        return;
    }

    for (size_t i = 0; i < d->sale_count; i++) {
        const Sale *s = &d->sales[i];
        const Car *c = NULL;
        const Customer *cu = NULL;

        for (size_t j = 0; j < d->car_count; j++)
            if (d->cars[j].id == s->car_id)
                c = &d->cars[j];

        for (size_t j = 0; j < d->customer_count; j++)
            if (d->customers[j].id == s->customer_id)
                cu = &d->customers[j];

        printf("Sale #%d | %s | %-20s | %-20s | $%.2f\n",
            s->id, s->date,
            cu ? cu->name : "Unknown customer",
            c ? c->model : "Archived vehicle",
            s->sale_price);
    }
}

static void dashboard(const Dealership *d) {
    size_t stock = 0;
    double stock_value = 0, total = 0, avg = 0;
    const Car *expensive = NULL;

    for (size_t i = 0; i < d->car_count; i++) {
        if (d->cars[i].available) {
            stock++;
            stock_value += d->cars[i].price;

            if (!expensive || d->cars[i].price > expensive->price)
                expensive = &d->cars[i];
        }
    }

    for (size_t i = 0; i < d->sale_count; i++)
        total += d->sales[i].sale_price;

    if (d->sale_count)
        avg = total / (double)d->sale_count;

    puts("\n                         DEALERSHIP DASHBOARD");
    line();

    printf("Active inventory       : %zu vehicles\n", stock);
    printf("Total vehicles listed  : %zu\n", d->car_count);
    printf("Registered customers   : %zu\n", d->customer_count);
    printf("Completed sales        : %zu\n", d->sale_count);
    printf("Lifetime sales revenue : AUD $%.2f\n", total);
    printf("Average sale value     : AUD $%.2f\n", avg);
    printf("Current stock value    : AUD $%.2f\n", stock_value);

    if (expensive)
        printf("Highest priced stock   : %s %s (AUD $%.2f)\n",
            expensive->make, expensive->model, expensive->price);

    line();
}

static void customers_report(const Dealership *d) {
    puts("\nCUSTOMER DIRECTORY");
    line();

    if (!d->customer_count) {
        puts("No customers recorded.");
        return;
    }

    for (size_t i = 0; i < d->customer_count; i++) {
        const Customer *c = &d->customers[i];

        printf("#%-4d %-24s %-30s %s\n",
            c->id, c->name, c->email, c->phone);
    }
}

static void menu(void) {
    puts("\n  1  Browse inventory       6  Update vehicle price");
    puts("  2  Search vehicles        7  Archive a listing");
    puts("  3  Sort inventory         8  Sales history");
    puts("  4  Register a vehicle     9  Customer directory");
    puts("  5  Sell a vehicle        10  Dealership dashboard");
    puts("  0  Save and exit");
}

int main(void) {
    Dealership d = {0};
    int choice;

    load_all(&d);

    puts("\n===============================================================");
    puts("       AUTOLOT PRO | DEALERSHIP MANAGEMENT SYSTEM");
    puts("===============================================================");
    puts("  Inventory | Customer CRM | Sales | Reporting | Persistence");

    for (;;) {
        menu();

        if (read_int("\n  Select an option: ", &choice) == 0)
            break;

        switch (choice) {
            case 1: list_cars(&d, 1); break;
            case 2: search_cars(&d); break;
            case 3: sort_cars(&d); break;
            case 4: add_car(&d); break;
            case 5: purchase(&d); break;
            case 6: update_price(&d); break;
            case 7: remove_car(&d); break;
            case 8: sales_history(&d); break;
            case 9: customers_report(&d); break;
            case 10: dashboard(&d); break;

            case 0:
                save_all(&d);
                puts("\nAll data saved. Thanks for using AutoLot Pro.");
                return 0;

            default:
                puts("Choose a menu option from 0 to 10.");
                break;
        }
    }

    save_all(&d);
    puts("\nAll data saved. Goodbye.");

    return 0;
}
