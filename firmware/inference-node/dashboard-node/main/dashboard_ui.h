const char dashboard_html[] = R"rawliteral(

<!DOCTYPE html>

<html>

<head>
<title>Predictive Maintenance</title>

<script>

var ws =
new WebSocket(
"ws://" +
location.host +
"/ws"
);

ws.onmessage =
function(event)
{
    let data =
        JSON.parse(event.data);

    document.getElementById(
        "score"
    ).innerHTML =
        data.anomaly;

    document.getElementById(
        "status"
    ).innerHTML =
        data.fault ?
        "FAULT" :
        "NORMAL";
}

</script>

</head>

<body>

<h1>
Machine Monitoring Dashboard
</h1>

<h2>
Anomaly Score:
<span id="score">0</span>
</h2>

<h2>
Status:
<span id="status">NORMAL</span>
</h2>

</body>

</html>

)rawliteral";