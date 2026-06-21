#include "esp_http_server.h"

extern const char dashboard_html[];

static esp_err_t root_handler(
    httpd_req_t *req)
{
    httpd_resp_send(
        req,
        dashboard_html,
        HTTPD_RESP_USE_STRLEN
    );

    return ESP_OK;
}

void start_webserver()
{
    httpd_handle_t server=NULL;

    httpd_config_t config=
        HTTPD_DEFAULT_CONFIG();

    httpd_start(
        &server,
        &config
    );

    httpd_uri_t uri={
        .uri="/",
        .method=HTTP_GET,
        .handler=root_handler
    };

    httpd_register_uri_handler(
        server,
        &uri
    );
}