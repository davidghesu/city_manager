#define _XOPEN_SOURCE 700  // posix extension for timestamp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>


typedef struct {
    int id;
    char inspector[32];
    double latitude;
    double longitude;
    char category[32];
    int severity;
    time_t timestamp;
    char description[256];
} Report;

char role[32];
char user[64];
char command[32];
char district[64];
int report_id = -1;
int threshold = -1;

void mode_to_string(const mode_t mode, char out[10]) {
    out[0] = (mode & S_IRUSR) ? 'r' : '-';
    out[1] = (mode & S_IWUSR) ? 'w' : '-';
    out[2] = (mode & S_IXUSR) ? 'x' : '-';
    out[3] = (mode & S_IRGRP) ? 'r' : '-';
    out[4] = (mode & S_IWGRP) ? 'w' : '-';
    out[5] = (mode & S_IXGRP) ? 'x' : '-';
    out[6] = (mode & S_IROTH) ? 'r' : '-';
    out[7] = (mode & S_IWOTH) ? 'w' : '-';
    out[8] = (mode & S_IXOTH) ? 'x' : '-';
    out[9] = '\0';
}

int check_permission(const char *path, const mode_t required_bit) {
    struct stat st;
    if (stat(path, &st) < 0) {
        fprintf(stderr, "Error stat '%s': %s\n", path, strerror(errno));
        return 0;
    }
    if (!(st.st_mode & required_bit)) {
        char perm[10];
        mode_to_string(st.st_mode, perm);
        fprintf(stderr, "Error: access denied for '%s' (permissions: %s)\n", path, perm);
        return 0;
    }
    return 1;
}

void log_action(const char *action) {
    char path[128];
    snprintf(path, sizeof(path), "%s/logged_district", district);

    if (strcmp(role, "inspector") == 0) {
        if (!check_permission(path, S_IWGRP)) return; 
    }

    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) { fprintf(stderr, "Error opening log: %s\n", strerror(errno)); return; }

    time_t now = time(NULL);
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "[%s] role=%s user=%s action=%s\n", strtok(ctime(&now), "\n"), role, user, action); // strtok for deleting the \n from ctime
    write(fd, buf, len);
    close(fd);
}

void create_file(const char *path, const mode_t perm, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, perm);
    if (fd >= 0) {
        if (content) write(fd, content, strlen(content));
        close(fd);
    }
    chmod(path, perm);
}

void create_district() {
    mkdir(district, 0750);
    chmod(district, 0750);

    char path[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district); create_file(path, 0664, NULL);
    snprintf(path, sizeof(path), "%s/district.cfg", district); create_file(path, 0640, "severity_threshold=2\n");
    snprintf(path, sizeof(path), "%s/logged_district", district); create_file(path, 0644, NULL);

    char symlink_name[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    snprintf(symlink_name, sizeof(symlink_name), "active_reports-%s", district);
    unlink(symlink_name);
    if (symlink(path, symlink_name) < 0) {
        fprintf(stderr, "Warning: failed to create symlink: %s\n", strerror(errno));
    }
}

void add() {

    struct stat stcreate;
    if (stat(district, &stcreate) < 0) { 
        create_district();
    }
    char path[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    mode_t required;
    if (strcmp(role, "manager") == 0) required = S_IWUSR;
    else required = S_IWGRP;

    if (!check_permission(path, required)) return;

    struct stat st;
    stat(path, &st);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error open: %s\n", strerror(errno));
        return;
    }

    Report r = {0};

    // get last report ID to assign a new unique ID
    Report tmp;
    if (lseek(fd, -sizeof(Report), SEEK_END) >= 0) read(fd, &tmp, sizeof(Report));
    else tmp.id = 0;
    r.id = tmp.id + 1;

    r.timestamp = time(NULL);
    strncpy(r.inspector, user, sizeof(r.inspector) - 1);

    printf("Category (road/lighting/flooding): ");
    fgets(r.category, sizeof(r.category), stdin);
    r.category[strcspn(r.category, "\n")] = '\0';

    printf("Severity (1/2/3): "); scanf("%d", &r.severity);
    if (r.severity < 1 || r.severity > 3) { fprintf(stderr, "Invalid severity.\n"); return; }
    printf("X: "); scanf("%lf", &r.latitude);
    printf("Y: "); scanf("%lf", &r.longitude);

    printf("Description: "); getchar(); // consume newline from previous scanf
    fgets(r.description, sizeof(r.description), stdin);
    r.description[strcspn(r.description, "\n")] = '\0';


    lseek(fd, 0, SEEK_END);
    write(fd, &r, sizeof(Report));
    close(fd);

    log_action(command);
    printf("Report #%d added.\n", r.id);
}


void list() {
    char path[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    mode_t required;
    if (strcmp(role, "manager") == 0) required = S_IRUSR;
    else required = S_IRGRP;
    if (!check_permission(path, required)) return;

    struct stat st;
    stat(path, &st);
 
    char permissions[10];
    mode_to_string(st.st_mode, permissions);
    printf("File: %s | Permissions: %s | Size: %lld bytes | Modified: %s",
        path, permissions, (long long)st.st_size, ctime(&st.st_mtime));
 
    int fd = open(path, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "Error opening file: %s\n", strerror(errno)); return; }
 
    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report))
        printf("  [%d] %s | %s | severity %d | %s",
            r.id, r.inspector, r.category, r.severity, ctime(&r.timestamp));
    
    log_action(command);
    close(fd);
}

void view() {
    char path[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
 
    mode_t required;
    if (strcmp(role, "manager") == 0) required = S_IRUSR;
    else required = S_IRGRP;
    if (!check_permission(path, required)) return;
 
    int fd = open(path, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "Error opening file: %s\n", strerror(errno)); return; }
 
    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.id == report_id) {
            printf("ID: %d\nInspector: %s\nCategory: %s\nSeverity: %d\nLatitude: %lf\nLongitude: %lf\nTimestamp: %sDescription: %s\n",
                   r.id, r.inspector, r.category, r.severity, r.latitude, r.longitude, ctime(&r.timestamp), r.description);
            log_action(command);
            close(fd);
            return;
        }
    }
 
    fprintf(stderr, "Report #%d not found.\n", report_id);
    log_action(command);
    close(fd);
}

void remove_report() {
    char path[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (strcmp(role, "manager") != 0) { 
        fprintf(stderr, "Error: manager only.\n"); 
        return; 
    }

    if (!check_permission(path, S_IWUSR)) return;
 

    int fd = open(path, O_RDWR);
    if (fd < 0) { fprintf(stderr, "Error opening file: %s\n", strerror(errno)); return; }
 
    struct stat st;
    stat(path, &st);
    int n = st.st_size / sizeof(Report); // number of reports
 
    // looking for the position of the report to remove
    int pos = -1;
    Report r;
    for (int i = 0; i < n; i++) {
        lseek(fd, i * sizeof(Report), SEEK_SET);
        read(fd, &r, sizeof(Report));
        if (r.id == report_id) { 
            pos = i; 
            break; 
        }
    }
 
    if (pos == -1) { 
        fprintf(stderr, "Report #%d not found.\n", report_id); 
        close(fd); 
        return; 
    }
 
    // shift all reports after the removed one up by one position
    for (int i = pos + 1; i < n; i++) {
        lseek(fd, i * sizeof(Report), SEEK_SET);
        read(fd, &r, sizeof(Report));
        lseek(fd, (i - 1) * sizeof(Report), SEEK_SET);
        write(fd, &r, sizeof(Report));
    }
 
    ftruncate(fd, (n - 1) * sizeof(Report));
    log_action(command);
    close(fd);
    printf("Report #%d removed.\n", report_id);
}


void update_threshold() {
    char path[128];
    snprintf(path, sizeof(path), "%s/district.cfg", district);

    if (strcmp(role, "manager") != 0) { 
        fprintf(stderr, "Error: manager only.\n"); 
        return; 
    }

 
    struct stat st;
    if (stat(path, &st) < 0) { fprintf(stderr, "Error stat: %s\n", strerror(errno)); return; }
 
    if ((st.st_mode & 0777) != 0640) {
        char permissions[10];
        mode_to_string(st.st_mode, permissions);
        fprintf(stderr, "Error: district.cfg permissions changed (expected rw-r-----, got %s).\n", permissions);
        return;
    }
 
    // delete old characters from the file
    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd < 0) { fprintf(stderr, "Error open: %s\n", strerror(errno)); return; }
 
    char buf[64];
    int string = snprintf(buf, sizeof(buf), "severity_threshold=%d\n", threshold);
    write(fd, buf, string);
    close(fd);
    log_action(command);
    printf("Threshold updated to %d.\n", threshold);
}


int parse_condition(const char *input, char *field, char *op, char *value) {
    const char *p1 = strchr(input, ':');
    if (!p1) return 0;

    const char *p2 = strchr(p1 + 1, ':');
    if (!p2) return 0;

    strncpy(field, input, p1 - input);
    field[p1 - input] = '\0';
    strncpy(op, p1 + 1, p2 - p1 - 1);
    op[p2 - p1 - 1] = '\0';
    strcpy(value, p2 + 1);

    return 1;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0 || strcmp(field, "timestamp") == 0) {
        long lhs = strcmp(field, "severity") == 0 ? r->severity : (long)r->timestamp;
        // special case for timestamp
        long rhs = 0;
        if (strcmp(field, "timestamp") == 0) {
            struct tm tm_struct = {0};
            if (strptime(value, "%Y-%m-%d", &tm_struct) != NULL) {
                rhs = (long)mktime(&tm_struct); // converts in seconds
            } else {
                rhs = atol(value);
            }
        } else {
            rhs = atol(value);
        }
        if (!strcmp(op, "==")) return lhs == rhs;
        if (!strcmp(op, "!=")) return lhs != rhs;
        if (!strcmp(op, "<"))  return lhs <  rhs;
        if (!strcmp(op, "<=")) return lhs <= rhs;
        if (!strcmp(op, ">"))  return lhs >  rhs;
        if (!strcmp(op, ">=")) return lhs >= rhs;
    } else {
        char *lhs = strcmp(field, "category") == 0 ? r->category : r->inspector;
        int cmp = strcmp(lhs, value);
        if (!strcmp(op, "==")) return cmp == 0;
        if (!strcmp(op, "!=")) return cmp != 0;
    }
    return 0;
}

void filter(int argc, char *argv[]) {
    char path[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    mode_t required;
    if (strcmp(role, "manager") == 0) required = S_IRUSR;
    else required = S_IRGRP;
    if (!check_permission(path, required)) return;

    char conditions[8][128];
    int  count = 0;
    for (int i = 1; i < argc; i++)
        if (!strncmp(argv[i], "--filter", 8)) {
            i += 2; // pass --filter and district
            while (i < argc && argv[i][0] != '-' && count < 8) {
                strncpy(conditions[count], argv[i], 127);
                conditions[count][127] = '\0';
                count++; i++;
            }
            break;
        }

    int fd = open(path, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "Error opening file: %s\n", strerror(errno)); return; }
    
    
    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int match = 1;
        for (int i = 0; i < count && match; i++) {
            char field[32], op[4], value[64];
            if (!parse_condition(conditions[i], field, op, value) || !match_condition(&r, field, op, value))
                match = 0;
        }
        if (match)
            printf("  [%d] %s | %s | severity %d | %s", r.id, r.inspector, r.category, r.severity, ctime(&r.timestamp));
    }
    log_action(command);
    close(fd);
}



int main(const int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Use: %s --role <r> --user <u> --add/--list/--view/--remove_report/--update_threshold/--filter <district> (<report_id> for view/remove || <value> for threshold || filter for filter)\n", argv[0]);
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) strncpy(role, argv[++i], sizeof(role) - 1);
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) strncpy(user, argv[++i], sizeof(user) - 1);
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) { strncpy(district, argv[++i], sizeof(district) - 1);
            strcpy(command, "add");
        }
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) { strncpy(district, argv[++i], sizeof(district) - 1);
            strcpy(command, "list");
        }
        else if (strcmp(argv[i], "--view") == 0 && i + 2 < argc) { strncpy(district, argv[++i], sizeof(district) - 1);
            report_id = atoi(argv[++i]); 
            strcpy(command, "view");
        }
        else if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) { strncpy(district, argv[++i], sizeof(district) - 1);
            report_id = atoi(argv[++i]); 
            strcpy(command, "remove_report");
        }
        else if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) { strncpy(district, argv[++i], sizeof(district) - 1);
            threshold = atoi(argv[++i]);
            strcpy(command, "update_threshold");
        }
        else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) { strncpy(district, argv[++i], sizeof(district) - 1);
            strcpy(command, "filter");
        }



    }

    if (role[0] == '\0' || user[0] == '\0' || command[0] == '\0' || district[0] == '\0' ) {
        fprintf(stderr, "Error: You must specify --role <r> --user <u> --add/--list/--view/--remove_report/--update_threshold--filter <district> (<report_id> for view/remove || <value> for threshold || filter for filter)\n");
        return 1;
    }

    if (strcmp(command, "add") == 0) {
        add();
    } else if (strcmp(command, "list") == 0) {
        list();
    } else if (strcmp(command, "view") == 0) {
        view();
    } else if (strcmp(command, "remove_report") == 0) {
        remove_report();
    } else if (strcmp(command, "update_threshold") == 0) {
        update_threshold();
    } else if (strcmp(command, "filter") == 0) {
        filter(argc, argv);
    } else {
        fprintf(stderr, "Error: Invalid command.\n");
    } 

    return 0;
}
