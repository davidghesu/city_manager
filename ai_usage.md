# AI Usage Documentation - Phase 1

## AI Tool Details
- **Tool**: Claude / Gemini

---

##  Implementare `parse_condition()` și `match_condition()`

### Promt
Vreau sa analizezi si sa tii cont de acest pdf pe parcursul raspunsului tau. Trebuie sa ma ajuti cu cele 2 functii: match_condition() si parse_condition. Acum analizeza pdf ul si scrie-mi codul in C pentru cele 2 functii.

---

### Funcția `parse_condition`
parse_condition sparge un string de forma field:operator:value în cele trei componente:

```c
int parse_condition(const char *input, char *field, char *op, char *value) {
    const char *p1 = strchr(input, ':');
    if (!p1) return 0;

    const char *p2 = strchr(p1 + 1, ':');
    if (!p2) return 0;

    strncpy(field, input,    p1 - input);     field[p1 - input]     = '\0';
    strncpy(op,    p1 + 1,   p2 - p1 - 1);   op[p2 - p1 - 1]       = '\0';
    strcpy(value,  p2 + 1);

    return 1;
}
```

### Funcția `match_condition`
match_condition verifică dacă un raport satisface condiția. Câmpurile numerice (severity, timestamp) se compară cu atol, cele string (category, inspector) cu strcmp:

```c
int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0 || strcmp(field, "timestamp") == 0) {
        long lhs = strcmp(field, "severity") == 0 ? r->severity : (long)r->timestamp;
        long rhs = atol(value);
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
```
---

## What I changed and why?

When I was testing the functions and asking Gemini for further clarification, I realized that for the timestamp filter, I had to type in a very long number (the seconds passed since 1970). This was hard to use. I asked Gemini to fix this so I can use normal dates instead, and it gave me this code:

```c
if (strcmp(field, "severity") == 0 || strcmp(field, "timestamp") == 0) {
    long lhs = (strcmp(field, "severity") == 0) ? r->severity : (long)r->timestamp;
    long rhs = 0; 

    if (strcmp(field, "timestamp") == 0) {
        struct tm tm_struct = {0};
        // Folosim un nume diferit de "tm" pentru a nu se confunda cu tipul structurii
        if (strptime(value, "%Y-%m-%d", &tm_struct) != NULL) {
            rhs = (long)mktime(&tm_struct);
        } else {
            // Dacă data nu e validă, încercăm totuși atol ca fallback
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
}
```
Alte două detalii importante:
Numele variabilei tm: În unele compilatoare, tm este deja numele unei structuri sau al unei funcții. Este mai sigur să folosești ceva de genul tm_val sau tm_struct.

#define _XOPEN_SOURCE: Funcția strptime nu face parte din standardul C de bază (este o extensie POSIX). Ca să nu primești erori la compilare, trebuie să pui #define _XOPEN_SOURCE chiar la începutul de tot al fișierului tău main.c, înainte de orice #include.

