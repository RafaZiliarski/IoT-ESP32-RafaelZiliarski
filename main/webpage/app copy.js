/**
 * Global variables
 */
var seconds = null;
var otaTimerVar = null;
var wifiConnectInterval = null;

$(document).ready(function(){
    getSSID();
    getUpdateStatus();
    startSensorInterval(); // Renomeado para ficar genérico
    startLocalTimeInterval();
    getConnectInfo();
    
    $("#connect_wifi").on("click", function(){
        checkCredentials();
    }); 
    
    $("#disconnect_wifi").on("click", function(){
        disconnectWifi();
    }); 
});   

/**
 * Gets updated sensor data
 */
function getSensorValues()
{
    $.getJSON('/dhtSensor.json', function(data) {
        // Atualiza Temperatura
        $("#temperature_reading").text(data.temp);
        
        // Atualiza Distância (Tenta ler 'distance', se não existir le 'humidity' legado)
        var dist = data.distance ? data.distance : data.humidity;
        $("#humidity_reading").text(dist);

        // Atualiza LED / Atuador
        var actuatorText = $("#actuator_reading");
        if (data.actuator == 1) {
            actuatorText.text("LIGADO");
            actuatorText.removeClass("off").addClass("on"); // Fica Vermelho
        } else {
            actuatorText.text("DESLIGADO");
            actuatorText.removeClass("on").addClass("off"); // Fica Verde
        }
    }).fail(function() {
        console.log("Erro ao obter dados dos sensores.");
    });
}

function startSensorInterval()
{
    setInterval(getSensorValues, 2000); // Atualiza a cada 2 segundos
}

/* =========================================
   WIFI & SYSTEM FUNCTIONS (Mantidas iguais, apenas ajustes visuais)
   ========================================= */

function getFileInfo() 
{
    var x = document.getElementById("selected_file");
    var file = x.files[0];
    document.getElementById("file_info").innerHTML = "Arquivo: " + file.name + "<br>Tamanho: " + (file.size/1024).toFixed(2) + " KB";
}

function updateFirmware() 
{
    var formData = new FormData();
    var fileSelect = document.getElementById("selected_file");
    
    if (fileSelect.files && fileSelect.files.length == 1) 
    {
        var file = fileSelect.files[0];
        formData.set("file", file, file.name);
        document.getElementById("ota_update_status").innerHTML = "Enviando " + file.name + ", aguarde...";
        document.getElementById("ota_update_status").className = "status-msg"; // Reset class

        var request = new XMLHttpRequest();
        request.upload.addEventListener("progress", updateProgress);
        request.open('POST', "/OTAupdate");
        request.responseType = "blob";
        request.send(formData);
    } 
    else 
    {
        window.alert('Selecione um arquivo .bin primeiro')
    }
}

function updateProgress(oEvent) 
{
    if (oEvent.lengthComputable) {
        getUpdateStatus();
    } 
}

function getUpdateStatus() 
{
    var xhr = new XMLHttpRequest();
    var requestURL = "/OTAstatus";
    xhr.open('POST', requestURL, false);
    xhr.send('ota_update_status');

    if (xhr.readyState == 4 && xhr.status == 200) 
    {       
        var response = JSON.parse(xhr.responseText);
        document.getElementById("latest_firmware").innerHTML = response.compile_date + " - " + response.compile_time

        if (response.ota_update_status == 1) 
        {
            seconds = 10;
            otaRebootTimer();
        } 
        else if (response.ota_update_status == -1)
        {
            document.getElementById("ota_update_status").innerHTML = "Erro no Upload!";
            document.getElementById("ota_update_status").style.color = "red";
        }
    }
}

function otaRebootTimer() 
{   
    document.getElementById("ota_update_status").innerHTML = "Sucesso! Reiniciando em: " + seconds + "s";
    if (--seconds == 0) {
        clearTimeout(otaTimerVar);
        window.location.reload();
    } else {
        otaTimerVar = setTimeout(otaRebootTimer, 1000);
    }
}

function stopWifiConnectStatusInterval()
{
    if (wifiConnectInterval != null) {
        clearInterval(wifiConnectInterval);
        wifiConnectInterval = null;
    }
}

function getWifiConnectStatus()
{
    var xhr = new XMLHttpRequest();
    var requestURL = "/wifiConnectStatus";
    xhr.open('POST', requestURL, false);
    xhr.send('wifi_connect_status');
    
    if (xhr.readyState == 4 && xhr.status == 200)
    {
        var response = JSON.parse(xhr.responseText);
        var statusDiv = document.getElementById("wifi_connect_status");
        
        statusDiv.innerHTML = "Conectando...";
        statusDiv.style.color = "#orange";
        
        if (response.wifi_connect_status == 2)
        {
            statusDiv.innerHTML = "Falha na conexão. Verifique a senha.";
            statusDiv.style.color = "red";
            stopWifiConnectStatusInterval();
        }
        else if (response.wifi_connect_status == 3)
        {
            statusDiv.innerHTML = "Conectado com Sucesso!";
            statusDiv.style.color = "green";
            stopWifiConnectStatusInterval();
            getConnectInfo();
        }
    }
}

function startWifiConnectStatusInterval()
{
    wifiConnectInterval = setInterval(getWifiConnectStatus, 2800);
}

function connectWifi()
{
    var selectedSSID = $("#connect_ssid").val();
    var pwd = $("#connect_pass").val();
    
    $.ajax({
        url: '/wifiConnect.json',
        dataType: 'json',
        method: 'POST',
        cache: false,
        headers: {'my-connect-ssid': selectedSSID, 'my-connect-pwd': pwd},
        data: {'timestamp': Date.now()}
    });
    
    startWifiConnectStatusInterval();
}

function checkCredentials()
{
    var errorList = "";
    var credsOk = true;
    var selectedSSID = $("#connect_ssid").val();
    var pwd = $("#connect_pass").val();
    
    if (selectedSSID == "") {
        errorList += "SSID não pode ser vazio.<br>";
        credsOk = false;
    }
    if (pwd == "") {
        errorList += "Senha não pode ser vazia.<br>";
        credsOk = false;
    }
    
    if (credsOk == false) {
        $("#wifi_connect_credentials_errors").html(errorList).css("color", "red");
    } else {
        $("#wifi_connect_credentials_errors").html("");
        connectWifi();    
    }
}

function showPassword()
{
    var x = document.getElementById("connect_pass");
    if (x.type === "password") {
        x.type = "text";
    } else {
        x.type = "password";
    }
}

function getConnectInfo()
{
    $.getJSON('/wifiConnectInfo.json', function(data)
    {
        $("#connected_ap").text(data["ap"]);
        $("#wifi_connect_ip").text(data["ip"]);
        $("#wifi_connect_netmask").text(data["netmask"]);
        $("#wifi_connect_gw").text(data["gw"]);
        
        // Mostra o card de info e esconde o botão de conectar se já estiver conectado (opcional)
        $("#ConnectInfo").show(); 
    });
}

function disconnectWifi()
{
    $.ajax({
        url: '/wifiDisconnect.json',
        dataType: 'json',
        method: 'DELETE',
        cache: false,
        data: { 'timestamp': Date.now() }
    });
    setTimeout("location.reload(true);", 2000);
}

function startLocalTimeInterval()
{
    setInterval(getLocalTime, 10000);
}

function getLocalTime()
{
    $.getJSON('/localTime.json', function(data) {
        $("#local_time").text(data["time"]);
    });
}

function getSSID()
{
    $.getJSON('/apSSID.json', function(data) {
        $("#ap_ssid").text(data["ssid"]);
    });
}