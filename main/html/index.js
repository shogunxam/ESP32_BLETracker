const updateViewMode = () => {
  const isMobile = window.innerWidth < 768;
  $("#devices-table").parent(".table-responsive").toggle(!isMobile);
  $("#devices-cards").toggle(isMobile);
};

$(() => { // Shorthand for $(document).ready()
  $(window).resize(updateViewMode);
  updateViewMode(); // Initial check
});

const confirmRestart = () => {
  if (confirm("Are you sure you want to restart the device?")) {
    window.location.href = '/restart';
  }
};