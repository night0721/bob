#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <curl/curl.h>

#include "bob.h"

void debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, GRN "[bob] " CRESET);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void error(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    fprintf(stderr, RED "[bob] " CRESET);
    vfprintf(stderr, msg, args);
    va_end(args);
}

/*
 * For curl to write data
 */
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

char *get_db_path()
{
    char *db_dir = getenv("XDG_DATA_HOME");
    char *db_path = malloc(PATH_MAX * sizeof(char));
    if (db_path == NULL) {
        error("Error allocating memory\n");
        exit(EXIT_FAILURE);
    }

    /* make db idr at .cache/bob if XDG_DATA_HOME isn't defined */
    if (db_dir) {
        snprintf(db_path, PATH_MAX, "%s/bob", db_dir);
    } else {
        snprintf(db_path, PATH_MAX, "%s/.cache/bob", getenv("HOME"));
    }

    /* create the directory if it doesn't exist */
    mkdir(db_path, 0755);
    strcat(db_path, "/bob.db");

    return db_path;
}

/*
 * act = 0 == delete
 * act = 1 == act
 */
int update_package_list(char *pkg, int act)
{
    char *db_path = get_db_path();
    FILE *db_file = fopen(db_path, "a+");
    if (db_file == NULL) {
		error("fopen: %s\n", strerror(errno));
        free(db_path);
        return 1;
    }

    char line[MAX_PKG_NAME_LENGTH];
	char **entries = malloc(MAX_PKGS * sizeof(char *));
	int entries_count = 0;
	if (entries == NULL) {
		error("Error allocating memory\n");
		free(db_path);
		return 1;
	}

	int found = 0;
    while (fgets(line, sizeof(line), db_file)) {
        line[strcspn(line, "\n")] = '\0';
		char *name = strdup(line);
		if (name == NULL) {
			error("Error allocating memory\n");
			free(db_path);
			free(entries);
			return 1;
		}
        if (strcmp(line, pkg) == 0) {
            found = 1;
			/* don't add to entries if we are looking for it */
			if (!act) continue;
		}
		entries[entries_count] = name;
		entries_count++;
    }

    if (act) {
		if (!found) {
			/* not duplicate and adding */
			fprintf(db_file, "%s\n", pkg);
		}
		goto done;
    } else {
		if (!found) {
			/* deleting but not here */
			error("Package %s is not installed\n", pkg);
			fclose(db_file);
			free(db_path);
			free(entries);
			return 1;
		} else {
			freopen(db_path, "w", db_file);
			for (int i = 0; i < entries_count; i++) {
				fprintf(db_file, "%s\n", entries[i]);
				free(entries[i]);
			}
		}
    }

done:
    fclose(db_file);
    free(db_path);
	free(entries);
	return 0;
}

int download_file(char *pkg)
{
    long response_code;

    char url[256], dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s%s", DEST_DIR, pkg);

    CURL *curl = curl_easy_init();
    if (curl) {
        FILE *dest_file = fopen(dest, "wb");
        if (dest_file == NULL) {
			error("fopen: %s\n", strerror(errno));
            return 1;
        }
        
        snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/%s", REPO, BRANCH, pkg);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, dest_file);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        int success = 0;
        if (res != CURLE_OK || response_code != 200) {
            error("Failed to download %s: %s (Error code %ld)\n", url, curl_easy_strerror(res), response_code);
            fclose(dest_file);
            remove(dest);
        } else {
            fclose(dest_file);
            debug("Downloaded %s to %s\n", url, dest);

            /* make binary exectuable */
            if (chmod(dest, 0755) != 0) {
                error("chmod: %s\n", strerror(errno));
            } else {
                debug("Made %s executable\n", pkg);
				/* add to package list */
				if (update_package_list(pkg, 1) == 0) {
					debug("Added %s to package list\n", pkg);
					success = 1;
				} else {
					error("Failed to add %s to package list\n", pkg);
				}
            }
        }

        curl_easy_cleanup(curl);
        if (!success) {
            return 1;
        } else {
            return 0;
        }
    }
    return 1;
}

int install_package(char *pkg)
{
	if (strcmp(pkg, "index") == 0) {
		error("'index' is reserved and cannot be installed\n");
	}
    debug("Installing %s\n", pkg);
    if (download_file(pkg) == 0) {
        debug("Installation completed\n");
		return 0;
    } else {
        error("Fail to install %s\n", pkg);
		return 1;
    }
}

int uninstall_package(char *pkg)
{
    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s%s", DEST_DIR, pkg);

	/* delete from package list */
	if (update_package_list(pkg, 0) == 0) {
		debug("Removed %s from package list\n", pkg);
	} else {
		error("Failed to remoe %s from package list\n", pkg);
		return 1;
	}
    debug("Uninstalling %s\n", pkg);
    if (remove(dest) == 0) {
        debug("Uninstalled %s\n", pkg);
    } else {
        error("remove: %s\n", strerror(errno));
		return 1;
    }
	return 0;
}

void update_package(char *pkg)
{
	if (uninstall_package(pkg) == 0) {
		install_package(pkg);
	} else {
		error("Cannot continue, please retry\n");
	}
}

char **search_index()
{
    long response_code;
    char temp_path[] = "/tmp/bob_index";

	char url[256];

    snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/index", REPO, BRANCH);

    CURL *curl = curl_easy_init();
    if (curl) {
        FILE *temp_f = fopen(temp_path, "wb");
        if (temp_f == NULL) {
			error("fopen: %s\n", strerror(errno));
            curl_easy_cleanup(curl);
            remove(temp_path);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, temp_f);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        fclose(temp_f);

        if (res != CURLE_OK || response_code != 200) {
            fprintf(stderr, "Failed to download repository index: %s (Error code %ld)\n", curl_easy_strerror(res), response_code);
            curl_easy_cleanup(curl);
            remove(temp_path);
            return NULL;
        }

        curl_easy_cleanup(curl);
    }

	FILE *index_file = fopen(temp_path, "r");
    if (index_file == NULL) {
		error("fopen: %s\n", strerror(errno));
        remove(temp_path);
        return NULL;
    }

	char **entries = malloc(MAX_PKGS * sizeof(char *));
	int entries_count = 0;

	if (entries == NULL) {
		error("Error allocating memory\n");
		return NULL;
	}
    char line[MAX_PKG_NAME_LENGTH];
    while (fgets(line, sizeof(line), index_file)) {
        line[strcspn(line, "\n")] = '\0';
		char *name = strdup(line);
		if (name == NULL) {
			error("Error allocating memory\n");
			return NULL;
		}
		entries[entries_count] = name;
		entries_count++;
    }
	entries[entries_count] = NULL;
    fclose(index_file);
    remove(temp_path);

    return entries;
}

void search_package(char *pkg)
{
    debug("Searching for package: %s\n", pkg);
	char **index = search_index();
	for (int i = 0; index[i] != NULL; i++) {
		if (strcmp(pkg, index[i]) == 0) {
			debug("Package \"%s\" found in repository index\n", pkg);
			return;
		}
	}
	error("Package \"%s\" not found in repository index\n", pkg);
}

void list_package(char *pkg)
{
	if (pkg == NULL) {
		/* list installed packages */
		char *db_path = get_db_path();
		FILE *db_file = fopen(db_path, "r");
		if (db_file == NULL) {
			error("fopen: %s\n", strerror(errno));
			free(db_path);
			return;
		}
		char line[MAX_PKG_NAME_LENGTH];
		
		while (fgets(line, sizeof(line), db_file)) {
			line[strcspn(line, "\n")] = '\0';
			printf("%s\n", line);
		}
	} else if (strcmp(pkg, "all") == 0) {
		/* list all available packages */
		char **index = search_index();
		for (int i = 0; index[i] != NULL; i++) {
			printf("%s\n", index[i]);
		}
	} else {
		error("Unknown option\n");
	}
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "bob %s\n\nUsage:\n", VERSION);
        fprintf(stderr, " (install|i|add) <package>\tInstall a package\n");
        fprintf(stderr, " (uninstall|d|del) <package>\tUninstall a package\n");
        fprintf(stderr, " (update|u) <package>\t\tUpdate a package\n");
        fprintf(stderr, " (search|s) <package>\t\tSearch for a package\n");
        fprintf(stderr, " (list|l) [all] \t\tList all packages installed/available\n");
        return EXIT_FAILURE;
    }
    char *command = argv[1];
    char *pkg = (argc > 2) ? argv[2] : NULL;

    if ((strcmp(command, "install") == 0 || strcmp(command, "add") == 0 || strcmp(command, "i") == 0) && pkg) {
        install_package(pkg);
    } else if ((strcmp(command, "uninstall") == 0 || strcmp(command, "del") == 0 || strcmp(command, "d") == 0) && pkg) {
        uninstall_package(pkg);
    } else if ((strcmp(command, "update") == 0 || strcmp(command, "u") == 0) && pkg) {
        update_package(pkg);
    } else if ((strcmp(command, "search") == 0 || strcmp(command, "s") == 0) && pkg) {
        search_package(pkg);
	} else if ((strcmp(command, "list") == 0 || strcmp(command, "l") == 0)) {
		list_package(pkg);
	} else {
        error("Unknown command or package name missing\n");
    }

    return EXIT_SUCCESS;
}

