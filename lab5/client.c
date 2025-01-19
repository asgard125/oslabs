#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"

void print_html_header() {
    printf("\n");
    printf("<html>\n");
    printf("<head>\n");
    printf("<title>Temperature Dashboard</title>\n");
    printf("<style>\n");
    printf("body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }\n");
    printf("header { background-color: #0078D7; color: white; padding: 20px; text-align: center; }\n");
    printf("nav { background-color: #f2f2f2; padding: 10px; text-align: center; margin-bottom: 20px; }\n");
    printf("nav a { margin: 0 15px; text-decoration: none; color: #0078D7; font-weight: bold; }\n");
    printf("nav a:hover { text-decoration: underline; }\n");
    printf("main { padding: 20px; text-align: center; }\n");
    printf("footer { background-color: #333; color: white; text-align: center; padding: 10px; position: fixed; bottom: 0; width: 100%; }\n");
    printf(".current-temp { font-size: 1.5em; font-weight: bold; margin-top: 20px; }\n");
    printf(".navigation { margin-bottom: 20px; font-size: 1.1em; }\n");
    printf(".navigation a { margin: 0 10px; color: #0078D7; text-decoration: none; padding: 8px 12px; border-radius: 5px; }\n");
    printf(".navigation a.active { background-color: #0078D7; color: white; }\n");
    printf(".navigation a:hover { text-decoration: underline; }\n");
    printf(".container { max-width: 800px; margin: 0 auto; padding: 20px; background-color: white; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }\n");
    printf(".footer-text { font-size: 0.9em; color: #bbb; }\n");
    printf("</style>\n");
    printf("</head>\n");
    printf("<body>\n");
    printf("<header>Welcome to Temperature Dashboard</header>\n");
    printf("<main>\n");
}

void print_html_navigation() {
    printf("\n<nav class=\"navigation\">\n");
    printf("<a href=\"/?time=last5\">Last 5 Minutes</a>\n");
    printf("<a href=\"/?time=hourly\">Hourly Average</a>\n");
    printf("<a href=\"/?time=daily\">Daily Average</a>\n");
    printf("</nav>\n");
}

void print_daily_navigation(const char *active_page) {
    printf("\n<nav class=\"navigation\">\n");
    printf("<a href=\"/daily/week\" class=\"%s\">Week</a>\n", strcmp(active_page, "/daily/week") == 0 ? "active" : "");
    printf("<a href=\"/daily/month\" class=\"%s\">Month</a>\n", strcmp(active_page, "/daily/month") == 0 ? "active" : "");
    printf("<a href=\"/daily/3months\" class=\"%s\">3 Months</a>\n", strcmp(active_page, "/daily/3months") == 0 ? "active" : "");
    printf("<a href=\"/daily/6months\" class=\"%s\">6 Months</a>\n", strcmp(active_page, "/daily/6months") == 0 ? "active" : "");
    printf("<a href=\"/daily/year\" class=\"%s\">Year</a>\n", strcmp(active_page, "/daily/year") == 0 ? "active" : "");
    printf("</nav>\n");
}

void print_hourly_navigation(const char *active_page) {
    printf("\n<nav class=\"navigation\">\n");
    printf("<a href=\"/hourly/day\" class=\"%s\">Day</a>\n", strcmp(active_page, "/hourly/day") == 0 ? "active" : "");
    printf("<a href=\"/hourly/week\" class=\"%s\">Week</a>\n", strcmp(active_page, "/hourly/week") == 0 ? "active" : "");
    printf("<a href=\"/hourly/month\" class=\"%s\">Month</a>\n", strcmp(active_page, "/hourly/month") == 0 ? "active" : "");
    printf("</nav>\n");
}

void print_secondly_navigation(const char *active_page) {
    printf("\n<nav class=\"navigation\">\n");
    printf("<a href=\"/secondly/1minute\" class=\"%s\">1 Min</a>\n", strcmp(active_page, "/secondly/1minute") == 0 ? "active" : "");
    printf("<a href=\"/secondly/5minutes\" class=\"%s\">5 Min</a>\n", strcmp(active_page, "/secondly/5minutes") == 0 ? "active" : "");
    printf("</nav>\n");
}

void print_html_footer() {
    printf("\n<footer>\n");
    printf("<p class=\"footer-text\">&copy; 2023 Temperature Dashboard</p>\n");
    printf("</footer>\n");
    printf("</body>\n");
    printf("</html>\n");
}

void print_current_temperature() {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open("temperature.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    const char *sql = "SELECT temp FROM temp_all ORDER BY date DESC LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    double curr_temp = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        curr_temp = sqlite3_column_double(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("<div class=\"current-temp\">Current Temperature: %.1f &deg;C</div>\n", curr_temp);
}

void print_daily_average(const char *active_page, const char *time_period) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    if (sqlite3_open("temperature.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    const char *sql_template = 
    "WITH numbered_data AS ("
    "    SELECT ROW_NUMBER() OVER (ORDER BY date DESC) AS row_num, "
    "           strftime('%Y-%m-%d', date) AS date, "
    "           avg_temp "
    "    FROM temp_day "
    ") "
    "SELECT row_num, date, avg_temp "
    "FROM numbered_data "
    "ORDER BY row_num "
    "LIMIT %s;"; // Placeholder for the limit based on time_period

    int limit = 0;
    if (strcmp(time_period, "week") == 0) {
        limit = 7;
    } else if (strcmp(time_period, "month") == 0) {
        limit = 30;
    } else if (strcmp(time_period, "3months") == 0) {
        limit = 90;
    } else if (strcmp(time_period, "6months") == 0) {
        limit = 180;
    } else if (strcmp(time_period, "year") == 0) {
        limit = 366;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), sql_template, limit);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    printf("<h2>Daily Average Temperature</h2>\n");
    print_daily_navigation(active_page);
    printf("<table>\n");
    printf("<tr><th>#</th><th>Date</th><th>Temperature (°C)</th></tr>\n");

    int row_num = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *date = (const char *)sqlite3_column_text(stmt, 1);
        double temp = sqlite3_column_double(stmt, 2);
        printf("<tr><td>%d</td><td>%s</td><td>%.1f</td></tr>\n", row_num++, date, temp);
    }

    printf("</table>\n");
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void handle_request(const char *request_uri) {
    // Check the requested page and call corresponding functions
    if (request_uri == NULL || strcmp(request_uri, "/") == 0) {
        return;
    } 

    if (strncmp(request_uri, "/hourly", 7) == 0) {
        print_hourly_navigation(request_uri);
    } else if (strncmp(request_uri, "/secondly", 9) == 0) {
        print_secondly_navigation(request_uri);
    } else if (strncmp(request_uri, "/daily", 6) == 0) {
        char time_period[20];
        sscanf(request_uri, "/daily/%s", time_period);
        print_daily_average(request_uri, time_period);
    }
}

int main() {
    print_html_header();
    print_current_temperature();
    print_html_navigation();

    char *request_uri = getenv("REQUEST_URI");
    handle_request(request_uri);

    print_html_footer();
    return 0;
}