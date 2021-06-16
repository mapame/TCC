window.smceeApiUrlBase = window.location.protocol + "//" + window.location.hostname + ":8081/";

window.smceeNavbarMenuItems = [
	{name: "Painel", href: "."},
	{name: "Administração", adminOnly: true, dropdown: [
		{name: "Aparelhos", href: "appliances.html"},
	]}
];
