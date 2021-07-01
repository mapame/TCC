window.smceeApiUrlBase = window.location.protocol + "//" + window.location.hostname + ((window.location.port === "") ? "/api/" : ":8081/");

window.smceeNavbarMenuItems = [
	{name: "Gráficos", dropdown: [
		{name: "Potência/Tensão", href: "power_chart.html"},
		{name: "Assinaturas", href: "signature_chart.html"},
		{name: null},
		{name: "Energia (minutos)", href: "energy_minute_chart.html"},
		{name: "Energia (horas)", href: "energy_hour_chart.html"},
		{name: "Energia (dias)", href: "energy_day_chart.html"},
	]},
	{name: "Administração", adminOnly: true, dropdown: [
		{name: "Usuários", href: "users.html"},
		{name: "Configurações", href: "configs.html"},
		{name: "Eventos do Medidor", href: "meter_events.html"},
		{name: null},
		{name: "Aparelhos", href: "appliances.html"},
		{name: "Assinaturas", href: "signatures.html"},
	]}
];
