const updateViewMode = () => {
  const isMobile = window.innerWidth < 768;
  $("#devices-table").parent(".table-responsive").toggle(!isMobile);
  $("#devices-cards").toggle(isMobile);
};

$(() => { // Shorthand for $(document).ready()
  loadData();
  $(window).resize(updateViewMode);
  updateViewMode(); // Initial check
});

const loadData = () => {
  getData('/getindexdata', null, 
    data => { if (!data.logs) $('#logs').hide(); }, // Concise success callback
    () => {} // Empty error callback (can be omitted if empty)
  );
};

const confirmRestart = () => {
  if (confirm("Are you sure you want to restart the device?")) {
    window.location.href = '/restart';
  }
};