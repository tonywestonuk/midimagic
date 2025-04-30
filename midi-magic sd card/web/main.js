(() => {


	const grid_canvas = document.getElementById("grid");
	const grid_ctx = grid_canvas.getContext("2d");

	const page_canvas = document.getElementById("page");
	const page_ctx = page_canvas.getContext("2d");

	const CMD_PAUSE = 0;
	const CMD_PLAY = 1;



	//const socket = new WebSocket(`ws://${window.location.hostname}/ws`);
	const evtSource = new EventSource("/sse");

	offset = -10;
	currentRow = 0;

	let updateWindow = (blocknum,rownum) => {

		grid_ctx.fillStyle = "black";
		grid_ctx.beginPath();
		grid_ctx.fillRect(0, 0, 1024, 1024);
		grid_ctx.stroke();

		grid_ctx.fillStyle = "#005000";
		grid_ctx.beginPath();
		grid_ctx.fillRect(70, 0, 470, 1024);
		grid_ctx.stroke();

		grid_ctx.fillStyle = "white";
		grid_ctx.font = "48px monospace";
		offset = rownum - 10;
		for (let i = 0; i < 21; i += 1) {
			if (offset + i >= 0 && offset + i < 32) {
				grid_ctx.fillText(i + offset, 8, 50 * i);
				for (j = 0; j < 3; j++) {

					
					grid_ctx.fillText(blocks.get(blocknum).music[offset + i][j], 558 + j * 159, 50 * i);
				}


				grid_ctx.fillText(blocks.get(blocknum).events[offset + i], 100, 50 * i);
			}
		}

		grid_ctx.strokeStyle = "grey";
		grid_ctx.lineWidth = 3;


		for (i = 3; i < 6; i++) {
			grid_ctx.beginPath(); // Start a new path
			grid_ctx.moveTo(74 + i * 159, 0);
			grid_ctx.lineTo(74 + i * 159, 1024);
			grid_ctx.stroke();
		}

		grid_ctx.beginPath(); // Start a new path
		grid_ctx.moveTo(74, 0);
		grid_ctx.lineTo(74, 1024);
		grid_ctx.stroke();

		grid_ctx.beginPath(); // Start a new path
		grid_ctx.moveTo(540, 0);
		grid_ctx.lineTo(540, 1024);
		grid_ctx.stroke();

		grid_ctx.fillStyle = "#ff000070";
		grid_ctx.beginPath();
		grid_ctx.fillRect(0, 457, 1024, 50);
		grid_ctx.stroke();

		document.getElementById("fld_blocknbr").innerHTML=blocknum;

	}

	const notes_string = ["C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"];



	let loadNewSong = function(song) {

		page_ctx.fillStyle = "#00000000";
		page_ctx.beginPath();
		page_ctx.fillRect(0, 0, 1024, 1024);

		document.getElementById("fld_name").innerText=song.name;


		blocks = new Map();

		song.blocks.forEach(block => {

			let music = [];
			let events = [];
			for (let i = 0; i < 32; i++) {
				music.push(["-----", "-----", "-----"]);
				events.push(["- ------------"]);
			}

			block.notes.forEach((note) => {
				let row = note[0];
				let col = note[1];
				let tone = note[2];
				let voice = note[3];
				let tone_string = notes_string[tone % 12] + Math.floor(tone / 12) + "" + voice + "0"

				if (tone == 0) {
					tone_string = "OFF";
				}


				music[row][col] = tone_string
			});

			block.events.forEach(event => {
				let row = event[0];
				let prev_rows = event[1];
				events[row] = "W " + event[2].wait;
			});

			theBlock = new Object();
			theBlock.music = music;
			theBlock.events = events;

			blocks.set(block.id,theBlock);

		})



	}


	document.getElementById("fullscreen")
		.addEventListener("click", () => document.body.requestFullscreen());


	evtSource.onmessage = (e) => {
		var data = JSON.parse(e.data);
		var keysArr = Object.keys(data);
		for (var i = 0; i < keysArr.length; i++) {
			switch (keysArr[i]) {
				case "row":
					currentRow = data.row;
					currentBlock = data.block;
					updateWindow(parseInt(currentBlock),currentRow);
					break;
				case "song":
					let uri = "tracks/" + data.song;
					console.log(uri);
					if (data.song == "") {
						return;
					}

					fetch(uri)
						.then(x => x.json())
						.then(y => {
							loadNewSong(y);
							updateWindow(0,0);

						});

					;
					break;
			}
		}
	};


	let setupGridDrag = () => {
		let drag = false;
		let dy = 0;


		grid_canvas.addEventListener('mousedown', (evt) => {
			dy = evt.y;
			drag = true;
		});
		grid_canvas.addEventListener('mouseup', () => drag = false);

		grid_canvas.addEventListener('mousemove', (evt) => {
			if (drag) {
				updateWindow(currentRow + Math.floor((dy - evt.y) / 20));
			}
		});

		grid_canvas.addEventListener('touchend', () => drag = false);



		grid_canvas.addEventListener('touchmove', (evt) => {
			evt.preventDefault(); // Prevent default scrolling behavior
			const touch = evt.touches[0]; // Get the first touch point
			const touchY = touch.clientY; // Get the Y coordinate of the touch

			if (drag == false) {
				dy = touchY;
				drag = true;
			}

			const offset = Math.floor((dy - touchY) / 20); // Calculate the offset based on your grid size (20px in this case)
			updateWindow(currentRow + offset);
		});
	}


	setupGridDrag();


})();