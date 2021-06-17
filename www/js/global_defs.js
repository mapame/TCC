window.smceeApiUrlBase = window.location.protocol + "//" + window.location.hostname + ":8081/";

window.smceeNavbarMenuItems = [
	{name: "Gráficos", dropdown: [
		{name: "Potência e Tensão", href: "power.html"},
		{name: null},
		{name: "Energia (minutos)", href: "energy_minute_chart.html"},
		{name: "Energia (horas)", href: "energy_hour_chart.html"},
		{name: "Energia (dias)", href: "energy_day_chart.html"},
	]},
	{name: "Administração", adminOnly: true, dropdown: [
		{name: "Usuários", href: "users.html"},
		{name: "Configurações", href: "config.html"},
		{name: "Eventos do Medidor", href: "events.html"},
		{name: null},
		{name: "Tarifas", href: "rates.html"},
		{name: "Aparelhos", href: "appliances.html"},
		{name: "Assinaturas", href: "signatures.html"},
	]}
];
