#include <iostream>
#include <string>
#include <libssh/libssh.h>
#include <curl/curl.h>
#include <webdriverxx/webdriver.h>
#include <fstream>
#include <chrono>
#include <thread>

using namespace webdriverxx;

class VRANDeployment {
public:
    static void deployVRAN() {
        ssh_session session = ssh_new();
        if (session == nullptr) {
            throw std::runtime_error("SSH session creation failed");
        }

        ssh_options_set(session, SSH_OPTIONS_HOST, "192.165.180.61");
        ssh_options_set(session, SSH_OPTIONS_USER, "admin");
        ssh_options_set(session, SSH_OPTIONS_PORT, &(int){22});
        ssh_options_set(session, SSH_OPTIONS_IDENTITY, "/home/jenkins/.ssh/id_rsa");

        int rc = ssh_connect(session);
        if (rc != SSH_OK) {
            ssh_free(session);
            throw std::runtime_error("SSH connection failed");
        }

        ssh_channel channel = ssh_channel_new(session);
        if (channel == nullptr) {
            ssh_disconnect(session);
            ssh_free(session);
            throw std::runtime_error("Channel creation failed");
        }

        rc = ssh_channel_open_session(channel);
        if (rc != SSH_OK) {
            ssh_channel_free(channel);
            ssh_disconnect(session);
            ssh_free(session);
            throw std::runtime_error("Channel session failed");
        }

        std::string cmd = "helm upgrade --install 5g-bts /home/admin/helm/vran-chart --namespace vran -f /home/admin/helm/values.yaml";
        rc = ssh_channel_request_exec(channel, cmd.c_str());
        
        char buffer[256];
        int nbytes;
        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0) {
            std::cout << "[REMOTE] " << std::string(buffer, nbytes);
        }

        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
    }

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static void uploadCommissioningFile() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("CURL initialization failed");
        }

        std::string readBuffer;
        std::ifstream file("commissioning_file.xml");
        std::string xml_content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/xml");

        curl_easy_setopt(curl, CURLOPT_URL, "https://192.165.180.61/api/commissioning/upload");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml_content.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            throw std::runtime_error("Failed to upload file");
        }

        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        std::cout << "Response Code: " << response_code << std::endl;
        std::cout << "Response Body: " << readBuffer << std::endl;

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    static void verifyStatus() {
        try {
            WebDriver driver = Start(Chrome());
            driver.Navigate("https://192.165.180.61/login");
            
            driver.FindElement(ByCss("#username")).SendKeys("admin");
            driver.FindElement(ByCss("#password")).SendKeys("nemuadmin");
            driver.FindElement(ByCss("#loginBtn")).Click();
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            std::string duStatus = driver.FindElement(
                ByXPath("//tr[td[contains(text(),'DU')]]/td[contains(@class, 'status')]")
            ).GetText();
            
            std::cout << "DU Status: " << duStatus << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
};

int main() {
    try {
        VRANDeployment::deployVRAN();
        VRANDeployment::uploadCommissioningFile();
        VRANDeployment::verifyStatus();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}