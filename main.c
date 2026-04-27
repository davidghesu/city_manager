#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

typedef struct {
    int    id;
    char   inspector[32];
    int id;
    char inspector[32];
    double latitude;
    double longitude;
    char   category[16];
    int    severity;
    char category[32];
    int severity;
    time_t timestamp;
    char   description[256];
    char description[256];
} Report;

char role[32];
char user[64];
char command[32];
char district[64];
int  report_id = -1;
int  threshold = -1;
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
    snprintf(path, sizeof(path), "%s/reports.dat",     district); create_file(path, 0664, NULL);
    snprintf(path, sizeof(path), "%s/district.cfg",    district); create_file(path, 0640, "severity_threshold=2\n");
    snprintf(path, sizeof(path), "%s/reports.dat", district); create_file(path, 0664, NULL);
    snprintf(path, sizeof(path), "%s/district.cfg", district); create_file(path, 0640, "severity_threshold=2\n");
    snprintf(path, sizeof(path), "%s/logged_district", district); create_file(path, 0644, NULL);

    char symlink_name[128];
    snprintf(path,         sizeof(path),        "%s/reports.dat",     district);
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    snprintf(symlink_name, sizeof(symlink_name), "active_reports-%s", district);
    unlink(symlink_name);
    symlink(path, symlink_name);
}

void add() {
    create_district();

    char path[128];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    mode_t required;
    if (strcmp(role, "manager") == 0) required = S_IWUSR;
    else required = S_IWGRP;

    if (!check_permission(path, required)) return;

    struct stat st;
    stat(path, &st);

    Report r = {0};
    r.id = st.st_size / sizeof(Report) + 1;
    r.timestamp = time(NULL);
    strncpy(r.inspector, user, sizeof(r.inspector) - 1);

    printf("Category (road/lighting/flooding): "); scanf("%15s",  r.category);
    printf("Category (road/lighting/flooding): ");
    fgets(r.category, sizeof(r.category), stdin);
    r.category[strcspn(r.category, "\n")] = '\0';

    printf("Severity (1/2/3): "); scanf("%d", &r.severity);
    if (r.severity < 1 || r.severity > 3) { fprintf(stderr, "Invalid severity.\n"); return; }
    printf("X: ");  scanf("%lf", &r.latitude);
    printf("X: "); scanf("%lf", &r.latitude);
    printf("Y: "); scanf("%lf", &r.longitude);
    printf("Description: ");   getchar();

    printf("Description: "); getchar(); // consume newline from previous scanf
    fgets(r.description, sizeof(r.description), stdin);
    r.description[strcspn(r.description, "\n")] = '\0';

    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        fprintf(stderr, "Error open: %s\n", strerror(errno));
        return;
    }
    write(fd, &r, sizeof(Report));
    close(fd);

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
            close(fd);
            return;
        }
    }
 
    fprintf(stderr, "Report #%d not found.\n", report_id);
    close(fd);
}


int main(const int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Use: %s --role <r> --user <u> --add/--list <district>\n", argv[0]);
        fprintf(stderr, "Use: %s --role <r> --user <u> --add/--list/--view <district> (<report_id> for view)\n", argv[0]);
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
    
    }

    if (role[0] == '\0' || user[0] == '\0' || command[0] == '\0') {
        fprintf(stderr, "Error: You must specify --role, --user and a command (add/list).\n");
    if (role[0] == '\0' || user[0] == '\0' || command[0] == '\0' || district[0] == '\0' ) {
        fprintf(stderr, "Error: You must specify --role, --user, --add/--list/--view, and a district (and report ID for view).\n");
        return 1;
    }

    if (strcmp(command, "add") == 0) {
        add();
    } else if (strcmp(command, "list") == 0) {
        // list();
        list();
    } else if (strcmp(command, "view") == 0) {
        view();
    } else {
        fprintf(stderr, "Error: Invalid command.\n");
    }
    } 

    return 0;
}