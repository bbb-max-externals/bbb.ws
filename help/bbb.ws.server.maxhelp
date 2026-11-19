{
  "patcher": {
    "fileversion": 1,
    "appversion": {
      "major": 8,
      "minor": 6,
      "revision": 4
    },
    "classnamespace": "box",
    "rect": [100, 100, 730, 400],
    "bglocked": 1,
    "default_fontsize": 12,
    "default_fontname": "Arial",
    "description": "bbb.ws.server receive modes",
    "boxes": [
      {
        "box": {
          "id": "obj-1",
          "maxclass": "comment",
          "text": "bbb.ws.server — receiving text",
          "patching_rect": [30, 20, 520, 22],
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": []
        }
      },
      {
        "box": {
          "id": "obj-2",
          "maxclass": "comment",
          "text": "Max keeps each distinct symbol until quit. Use @textmode bytes for LAN or untrusted peers.",
          "patching_rect": [30, 52, 645, 22],
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": []
        }
      },
      {
        "box": {
          "id": "obj-3",
          "maxclass": "comment",
          "text": "This example listens on loopback. Open the client help and connect; send hi from the client.",
          "patching_rect": [30, 82, 665, 22],
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": []
        }
      },
      {
        "box": {
          "id": "obj-4",
          "maxclass": "message",
          "text": "broadcast hi",
          "patching_rect": [30, 122, 110, 22],
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""]
        }
      },
      {
        "box": {
          "id": "obj-5",
          "maxclass": "message",
          "text": "textmode symbol",
          "patching_rect": [165, 122, 112, 22],
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""]
        }
      },
      {
        "box": {
          "id": "obj-6",
          "maxclass": "message",
          "text": "textmode bytes",
          "patching_rect": [290, 122, 112, 22],
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""]
        }
      },
      {
        "box": {
          "id": "obj-7",
          "maxclass": "message",
          "text": "stop",
          "patching_rect": [425, 122, 82, 22],
          "numinlets": 2,
          "numoutlets": 1,
          "outlettype": [""]
        }
      },
      {
        "box": {
          "id": "obj-8",
          "maxclass": "newobj",
          "text": "bbb.ws.server @bind_ip 127.0.0.1 @port 8080",
          "patching_rect": [30, 179, 390, 22],
          "numinlets": 1,
          "numoutlets": 2,
          "outlettype": ["", ""]
        }
      },
      {
        "box": {
          "id": "obj-9",
          "maxclass": "newobj",
          "text": "print bbb.ws.server.data",
          "patching_rect": [30, 232, 170, 22],
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": []
        }
      },
      {
        "box": {
          "id": "obj-10",
          "maxclass": "newobj",
          "text": "print bbb.ws.server.status",
          "patching_rect": [360, 232, 175, 22],
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": []
        }
      },
      {
        "box": {
          "id": "obj-11",
          "maxclass": "comment",
          "text": "From client 1: hi → 1 hi (default) or 1 text 104 105 (bytes). Binary keeps its ID and byte list.",
          "patching_rect": [30, 280, 670, 22],
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": []
        }
      },
      {
        "box": {
          "id": "obj-12",
          "maxclass": "comment",
          "text": "@max_message_size defaults to 1 MiB. Byte lists too long for Max are dropped, never truncated; see the status outlet.",
          "patching_rect": [30, 313, 655, 48],
          "numinlets": 1,
          "numoutlets": 0,
          "outlettype": []
        }
      }
    ],
    "lines": [
      {
        "patchline": {
          "source": ["obj-4", 0],
          "destination": ["obj-8", 0]
        }
      },
      {
        "patchline": {
          "source": ["obj-5", 0],
          "destination": ["obj-8", 0]
        }
      },
      {
        "patchline": {
          "source": ["obj-6", 0],
          "destination": ["obj-8", 0]
        }
      },
      {
        "patchline": {
          "source": ["obj-7", 0],
          "destination": ["obj-8", 0]
        }
      },
      {
        "patchline": {
          "source": ["obj-8", 0],
          "destination": ["obj-9", 0]
        }
      },
      {
        "patchline": {
          "source": ["obj-8", 1],
          "destination": ["obj-10", 0]
        }
      }
    ]
  }
}
