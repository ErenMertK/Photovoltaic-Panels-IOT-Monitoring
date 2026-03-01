// 1. Firebase Kütüphanelerini İçe Aktar (CDN üzerinden)
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.13.1/firebase-app.js";
import { getDatabase, ref, onValue } from "https://www.gstatic.com/firebasejs/10.13.1/firebase-database.js";
import { getAuth, signInWithEmailAndPassword, signOut, onAuthStateChanged } from "https://www.gstatic.com/firebasejs/10.13.1/firebase-auth.js";

// 2. Senin Proje Yapılandırma Bilgilerin (Güncellendi ✅)
const firebaseConfig = {
  apiKey: "AIzaSyCbO8Cj5nX05T3nYH0qI7crZyq5qZe1L3w",
  authDomain: "bitirme-projesi-f5c5d.firebaseapp.com",
  databaseURL: "https://bitirme-projesi-f5c5d-default-rtdb.europe-west1.firebasedatabase.app",
  projectId: "bitirme-projesi-f5c5d",
  storageBucket: "bitirme-projesi-f5c5d.firebasestorage.app",
  messagingSenderId: "480418994148",
  appId: "1:480418994148:web:5e333be6cbc605baa799f1"
};

// 3. Uygulamayı Başlat
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);   // Veritabanı servisi
const auth = getAuth(app);     // Kimlik doğrulama servisi

// 4. HTML Sayfaları İçin Yardımcı Fonksiyonlar

// Veritabanından veri okuma kısayolu
export const onData = (path, callback) => {
    const reference = ref(db, path);
    onValue(reference, callback);
};

// Kimlik doğrulama fonksiyonlarını dışarı aktar
export { auth, signInWithEmailAndPassword, signOut, onAuthStateChanged };