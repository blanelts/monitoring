// utils.js
export function formatLastSeen(seconds) {
    if (seconds < 60) return seconds + " sec ago";
    else if (seconds < 3600) return Math.floor(seconds / 60) + " min ago";
    else if (seconds < 86400) return Math.floor(seconds / 3600) + " hr ago";
    else if (seconds < 604800) return Math.floor(seconds / 86400) + " days ago";
    else if (seconds < 2592000) return Math.floor(seconds / 604800) + " weeks ago";
    else if (seconds < 31536000) return Math.floor(seconds / 2592000) + " months ago";
    else return Math.floor(seconds / 31536000) + " years ago";
  }
  