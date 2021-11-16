window.smceeApiUrlBase = window.location.protocol + "//" + window.location.hostname + ((window.location.port === "") ? "/api/" : ":8081/");

window.smceeNavbarMenuItems = [
	{name: "Consumo", href: "consumption.html"},
	{name: "Gráficos", dropdown: [
		{name: "Potência e Tensão", href: "power_chart.html"},
		{name: "Energia", href: "energy_chart.html"},
		{name: "Energia (minutos)", href: "energy_chart_minutes.html"},
		{name: "Assinaturas", href: "signature_chart.html"},
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
