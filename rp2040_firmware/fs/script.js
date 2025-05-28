document.addEventListener('DOMContentLoaded', () => {
    const gbStatusSpan = document.getElementById('gb-status');
    const pokemonListUl = document.getElementById('pokemon-list');
    const selectedPokemonSpan = document.getElementById('selected-pokemon-trade');
    const initiateTradeBtn = document.getElementById('initiate-trade-btn');
    const logOutputPre = document.getElementById('log-output');

    function logMessage(message) {
        logOutputPre.innerText += message + '\n';
        logOutputPre.scrollTop = logOutputPre.scrollHeight;
    }

    function fetchStatus() {
        logMessage('Fetching GameBoy status...');
        fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            gbStatusSpan.textContent = data.status || 'Error';
            logMessage('Status updated: ' + (data.status || 'Error fetching status'));
        })
        .catch(error => {
            gbStatusSpan.textContent = 'Error';
            logMessage('Error fetching status: ' + error);
        });
    }

    function fetchStoredPokemon() {
        logMessage('Fetching stored Pokemon list...');
        fetch('/api/pokemon/list')
        .then(response => response.json())
        .then(data => {
            pokemonListUl.innerHTML = ''; // Clear list
            if (data && Array.isArray(data)) {
                data.forEach(p => {
                    const li = document.createElement('li');
                    if (p.valid) {
                        li.textContent = `Slot ${p.slot + 1}: ${p.name || ('Pokemon ' + (p.species_id || 'Unknown'))} (Gen ${p.gen || 'N/A'}, Lvl ${p.level || 'N/A'})`;
                        // Add click listener to select pokemon for trade
                        li.addEventListener('click', () => selectPokemonForTrade(p, p.slot + 1));
                        li.style.cursor = 'pointer';
                    } else {
                        li.textContent = `Slot ${p.slot + 1}: Empty`;
                    }
                    pokemonListUl.appendChild(li);
                });
            } else {
                 logMessage('Pokemon list data is not an array or is empty.');
            }
            logMessage('Pokemon list updated.');
        })
        .catch(error => {
            logMessage('Error fetching Pokemon list: ' + error);
        });
    }
    
    let currentSelectedPokemon = null;

    function selectPokemonForTrade(pokemon, slotNumber) {
        currentSelectedPokemon = { ...pokemon, slot: slotNumber -1 }; // Store 0-indexed slot
        selectedPokemonSpan.textContent = `${pokemon.name || 'Unknown Pokemon'} (from Slot ${slotNumber})`;
        logMessage(`Selected for trade: ${pokemon.name || 'Unknown Pokemon'} from Slot ${slotNumber}`);
    }

    function initiateTrade() {
        if (!currentSelectedPokemon) {
            logMessage('No Pokemon selected for trade.');
            alert('Please select a Pokemon to trade first.');
            return;
        }
        logMessage(`Initiating trade with GameBoy for ${currentSelectedPokemon.name}...`);
        
        const formData = new URLSearchParams();
        formData.append('slot', currentSelectedPokemon.slot);

        fetch('/api/trade/start', { 
            method: 'POST', // Using POST
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded', // Standard for form data
            },
            body: formData.toString() // Convert params to string
        })
        .then(response => response.json())
        .then(data => {
            logMessage('Trade initiated: ' + data.message);
            alert('Trade Response: ' + data.message);
        })
        .catch(error => {
            logMessage('Error initiating trade: ' + error);
            alert('Error initiating trade.');
        });
    }

    if (initiateTradeBtn) {
        initiateTradeBtn.addEventListener('click', initiateTrade);
    }

    // Initial fetches
    fetchStatus();
    fetchStoredPokemon();

    logMessage('Pokemon Link UI Initialized.');
});
