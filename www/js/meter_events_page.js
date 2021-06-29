window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("timespan-select").onchange = updateTimespan;
	document.getElementById("event-type-select").onchange = updateEventTable.bind(null, 1);
	
	window.smceeMeterEventTimespan = Number(document.getElementById("timespan-select").value);
	fetchEvents(window.smceeMeterEventTimespan);
}

function eventTableAddItem(item) {
	var tableNewRow = document.createElement("tr");
	var tableCellDate = document.createElement("td");
	var tableCellType = document.createElement("td");
	
	document.getElementById("meter-events-tbody").appendChild(tableNewRow);
	
	tableNewRow.appendChild(tableCellDate);
	tableNewRow.appendChild(tableCellType);
	
	tableCellDate.className = "has-text-centered is-vcentered";
	tableCellType.className = "has-text-centered is-vcentered is-size-7";
	
	tableCellDate.innerText = new Date(item.timestamp * 1000).toLocaleString("pt-BR");
	tableCellType.innerText = item.type + ((item.count > 1) ? (" (x" + item.count + ")") : "");
}

function updateEventTable(page=1) {
	var tableBody = document.getElementById("meter-events-tbody");
	var eventType = document.getElementById("event-type-select").value;
	var filteredEvents;
	var pageCount;
	
	if(typeof window.smceeMeterEvents != "object")
		return;
	
	if(eventType === "all")
		filteredEvents = window.smceeMeterEvents;
	else
		filteredEvents = window.smceeMeterEvents.filter(event => event.type === eventType);
	
	pageCount = Math.max(1, Math.ceil(filteredEvents.length / 16));
	
	if(page > pageCount)
		page = pageCount;
	
	while(tableBody.lastChild)
		tableBody.removeChild(tableBody.lastChild);
	
	for(let eventIndex = (page - 1) * 16; eventIndex < page * 16 && eventIndex < filteredEvents.length; eventIndex++)
		eventTableAddItem(filteredEvents[eventIndex]);
	
	updateEventPagination(pageCount, page);
}

function updateTypeFilterSelect() {
	var selectElement = document.getElementById("event-type-select");
	var meterEventTypes = new Map();
	
	if(typeof window.smceeMeterEvents != "object")
		return;
	
	for(const event of window.smceeMeterEvents)
		meterEventTypes.set(event.type, (meterEventTypes.has(event.type) ? meterEventTypes.get(event.type) : 0) + event.count);
	
	while(selectElement.childElementCount > 1)
		selectElement.removeChild(selectElement.lastChild);
	
	selectElement.value = "all";
	
	for(const [typeName, typeCount] of meterEventTypes) {
		selectOption = document.createElement("option");
		selectOption.innerText = typeName + " (" + typeCount + ")";
		selectOption.value = typeName;
		
		selectElement.appendChild(selectOption);
	}
}

function updateEventPagination(pageCount, currentPage) {
	var paginationListElement = document.getElementById("event-pagination");
	
	if(pageCount < 1 || currentPage < 1 || currentPage > pageCount)
		return;
	
	while(paginationListElement.childElementCount > 0)
		paginationListElement.removeChild(paginationListElement.lastChild);
	
	for(let pageIndex = 1; pageIndex <= pageCount; pageIndex++) {
		let pageElement = document.createElement("li");
		let pageLinkElement = document.createElement("a");
		
		pageLinkElement.className = "pagination-link";
		pageLinkElement.innerText = pageIndex;
		pageLinkElement.onclick = updateEventTable.bind(null, pageIndex);
		
		if(pageIndex == currentPage)
			pageLinkElement.classList.add("is-current");
		
		pageElement.appendChild(pageLinkElement);
		paginationListElement.appendChild(pageElement);
		
		if((currentPage > 3 && pageIndex == 1) || (pageCount - currentPage >= 3 && pageIndex == currentPage + 1)) {
			let pageEllipsisElement = document.createElement("li");
			let pageEllipsisInnerElement = document.createElement("span");
			
			pageEllipsisInnerElement.className = "pagination-ellipsis";
			pageEllipsisInnerElement.innerHTML = "&hellip;";
			
			pageEllipsisElement.appendChild(pageEllipsisInnerElement);
			paginationListElement.appendChild(pageEllipsisElement);
			
			if(pageIndex == 1 && currentPage > 3)
				pageIndex = currentPage - 2;
			else if(pageIndex == currentPage + 1)
				pageIndex = pageCount - 1;
		}
	}
}

function updateTimespan() {
	var newTimespan = Number(document.getElementById("timespan-select").value);
	
	if(typeof window.smceeMeterEvents != "object")
		return;
	
	if(newTimespan > window.smceeMeterEventTimespan) {
		fetchEvents(newTimespan);
	} else if(window.smceeMeterEvents.length > 0) {
		let startTimestamp = window.smceeMeterEvents[0].timestamp - newTimespan;
		window.smceeMeterEvents = window.smceeMeterEvents.filter(event => event.timestamp >= startTimestamp);
		updateTypeFilterSelect();
		updateEventTable();
	}
	
	window.smceeMeterEventTimespan = newTimespan;
}

function fetchEvents(timespan) {
	var xhrFetchEvents = new XMLHttpRequest();
	
	document.getElementById("timespan-select").disabled = true;
	document.getElementById("event-type-select").disabled = true;
	
	document.getElementById("timespan-select").parentNode.classList.add("is-loading");
	document.getElementById("event-type-select").parentNode.classList.add("is-loading");
	
	xhrFetchEvents.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			window.smceeMeterEvents = [];
			
			for(const event of responseObject)
				window.smceeMeterEvents.push(event);
			
			updateTypeFilterSelect();
			updateEventTable();
			
			document.getElementById("timespan-select").disabled = false;
			document.getElementById("event-type-select").disabled = false;
			
			document.getElementById("timespan-select").parentNode.classList.remove("is-loading");
			document.getElementById("event-type-select").parentNode.classList.remove("is-loading");
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch meter events. Status: " + this.status);
		}
	}
	
	xhrFetchEvents.open("GET", window.smceeApiUrlBase + "meter/events?last=" + timespan);
	
	xhrFetchEvents.timeout = 2000;
	
	xhrFetchEvents.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrFetchEvents.send();
}
