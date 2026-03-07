#include <iostream>
#include <iomanip>
#include <ctime>
#include <mutex>

using namespace std;

mutex api_mutex;

class ApiClient {
private:
    mutex api_mutex;

    chrono::steady_clock::time_point lastCall = chrono::steady_clock::now() - chrono::seconds(1);
    const chrono::milliseconds interval{500};
public:
    string query_scryfall(string query) {
        lock_guard lock(api_mutex);

        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastCall);

        if(elapsed < interval) {
            // Sleep until interval is done
        }

        result = "Querying API: " + query + "...";
        log_info(result);

        lastCall = chrono::steady_clock::now();
        return result;
    }
}

void log_info(const string& message);
void log_error(const string& message);
string atomic_query(string query);

int main() {
    // loop and check redis queue

    // after intake number met or time increment, execute api query and put results in database

    // Will also need to handle other async tasks with new threads
    // ensure api query function is atomic to respect rate limit for scryfall

    // go back to looping
    quit();
}

void log_info(const string& message) {
    auto now = time(nullptr);
    cout << "[INFO] " << put_time(localtime(&now), "%Y-%m-%d %H:%M:%S") 
              << " - " << message << endl;
}

void log_error(const string& message) {
    auto now = time(nullptr);
    cerr << "[ERROR] " << put_time(localtime(&now), "%Y-%m-%d %H:%M:%S") 
              << " - " << message << endl;
}