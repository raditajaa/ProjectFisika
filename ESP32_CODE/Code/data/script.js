/*
============================================================
AMBIL ELEMENT HTML
============================================================
*/
const statusText = document.getElementById("statusText");
const progressBar = document.getElementById("progressBar");
const doorLink = document.getElementById("doorLink");
const startButton = document.getElementById("startButton");

/*
============================================================
UPDATE STATUS
============================================================
*/
async function updateStatus() {
    try {
        // Request ke ESP32
        const response = await fetch("/status");

        // Cek response
        if (!response.ok) {
            throw new Error(`HTTP Error: ${response.status}`);
        }

        // Ubah response menjadi JSON
        const data = await response.json();

        // UPDATE PROGRESS
        progressBar.style.width = `${data.progress}%`;
        progressBar.innerText = `${data.progress}%`;

        // UPDATE STATUS
        statusText.innerText = data.status;

        // UPDATE TOMBOL PINTU
        doorLink.style.display = data.pintu ? "block" : "none";

        // UPDATE TOMBOL START
        startButton.disabled = data.progress > 0 && data.progress < 100;

    } catch (error) {
        console.error("Gagal mengambil status ESP32:", error);
    }
}

/*
============================================================
INTERVAL & INITIAL CALL
============================================================
*/
// Update setiap 500ms
setInterval(updateStatus, 500);

// Update pertama kali
updateStatus();