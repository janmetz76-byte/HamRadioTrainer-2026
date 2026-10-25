// HamRadioTrainer 2026 – hrt2026.de
// Menü auf schmalen Bildschirmen, Reiter der Bildschirmfotos und eine
// Vergrößerung beim Anklicken. Ohne Skript bleibt alles bedienbar: die
// Bilder öffnen sich dann einfach in voller Größe.

(function () {
  "use strict";

  // Menü
  const toggle = document.querySelector(".nav-toggle");
  const nav = document.getElementById("nav");
  if (toggle && nav) {
    toggle.addEventListener("click", function () {
      const open = nav.classList.toggle("open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    nav.addEventListener("click", function (event) {
      if (event.target.tagName === "A") {
        nav.classList.remove("open");
        toggle.setAttribute("aria-expanded", "false");
      }
    });
  }

  // Reiter Windows / Linux / Android
  const tabs = Array.from(document.querySelectorAll('[role="tab"]'));
  tabs.forEach(function (tab) {
    tab.addEventListener("click", function () {
      tabs.forEach(function (other) {
        const selected = other === tab;
        other.setAttribute("aria-selected", selected ? "true" : "false");
        document.getElementById(other.getAttribute("aria-controls")).hidden = !selected;
      });
    });
  });

  // Vergrößerung
  const box = document.querySelector("dialog.lightbox");
  if (!box || typeof box.showModal !== "function") return;
  const image = box.querySelector("img");
  const caption = box.querySelector("p");
  document.querySelectorAll(".gallery a").forEach(function (link) {
    link.addEventListener("click", function (event) {
      event.preventDefault();
      const thumb = link.querySelector("img");
      image.src = link.getAttribute("href");
      image.alt = thumb ? thumb.alt : "";
      const figcaption = link.parentElement.querySelector("figcaption");
      caption.textContent = figcaption ? figcaption.textContent : "";
      box.showModal();
    });
  });
  box.querySelector("button").addEventListener("click", function () { box.close(); });
  box.addEventListener("click", function (event) {
    if (event.target === box) box.close();
  });
})();
