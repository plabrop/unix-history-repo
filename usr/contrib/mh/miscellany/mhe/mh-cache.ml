; This file holds functions that create and manipulate the cache of header
; information for the various message files. It is explicitly loaded from
; the root.
-------------------------------------------------------------------------

; This function creates the header buffer that represents a message or
; bboard directory. It associates several buffer-specific variables
; with it: mh-buffer-filename, which is the actual Unix file name of the
; directory; mh-folder-title, which is either the tail of the directory
; name or the whole thing depending on whether it is in your mail path.
; call: (&mh-read-folder "folder" "range" "pathname" "title")
(defun 
    (&mh-read-folder name rnge title
	(setq name (arg 1)) (setq rnge (arg 2))
	(pop-to-buffer (concat "+" mh-folder))
	(if buffer-is-modified
	    (write-current-file))
	(setq mh-folder name)
	(switch-to-buffer (concat "+" name))
	(setq backup-before-writing 0)
	(setq wrap-long-lines 0)
	(use-local-map "&mh-keymap")
	(setq mode-string "mh-folder")
	(if (= (buffer-size) 0)
	    (progn 
		   (if (!= 0 (length mh-flist))
		       (setq mh-flist (concat mh-flist ",")))
		   (setq mh-flist (concat mh-flist name))
		   (setq mh-buffer-filename (arg 3))
		   (setq mh-folder-title (arg 4))
		   (use-local-map "&mh-keymap")
		   (if (error-occured 
			   (read-file (concat mh-buffer-filename "/"
					      (current-buffer-name))))
		       (progn 
			      (message "Header file missing;  regenerating it...")
			      (sit-for 0)
			      (&mh-regenerate-headers)
		       )
		       (progn 
			      (&mh-update-headers)
		       )
		   )
		   (if (looking-at "scan: "); UCI
;UCI		       (looking-at "No messages ")
		       (progn 
			      (if (= rnge "")
				  (message  "Folder +" name " is empty.")
				  (message  "No messages in +" name " range " rnge)
;UCI				  (erase-buffer)
			      )
			      (sit-for 15)
			      (erase-buffer); UCI
		       )
		   )
		   (setq mode-line-format
			 (concat "{%b} %[%] "
				 "Cmds: n p d ^ ! u t e m f i r g b x ?  Exit:^X^C   %M")
		   )
		   (&mh-check-folder-full)
	    )
	)
	(&mh-adjust-window)
	(setq buffer-is-modified 0)
    )
)

(defun
    (&mh-check-folder-full lastmsg
	(save-excursion
	    (temp-use-buffer (concat "+" mh-folder))
	    (end-of-file)
	    (previous-line)
	    (beginning-of-line)
	    (while (= (following-char) ' ') (forward-character))
	    (set-mark)
	    (beginning-of-line)
	    (goto-character (+ (dot) 3))
	    (setq lastmsg (region-to-string))
	    (if (> lastmsg 900)
		(progn ans
		       (setq ans 
			     (get-response (concat "Folder +" mh-folder " is >90%% full. May I pack it for you? ")
				 "yYnN\"
				 "Please answer y or n"))
		       (if (= ans 'y')
			   (progn
				 (&mh-pack-folder)
			   )
			   (progn
				 (message "OK, but you should use the 'x-p' command to pack it soon.")
				 (sit-for 20)
			   )
		       )
		)
	    )
	)
    )
    
    (&mh-adjust-window
	(&mh-unmark-all-headers 0)
	(&mh-position-to-current)
	(save-excursion 
	    (beginning-of-window)
	    (if (! (bobp))
		(progn t
		       (end-of-file)
		       (setq t (dot))
		       (while (= t (dot))
			      (progn 
				     (scroll-one-line-down)
				     (sit-for 0)
			      ))
		       (scroll-one-line-up)
		)
	    )
	)
    )
)

(defun 
    (&mh-regenerate-headers
	(setq mode-line-format " please wait for header regeneration...")
	(message  "scan +" mh-folder-title)
	(sit-for 0)
	(erase-buffer) (set-mark)
	(fast-filter-region  (concat mh-progs "/scan +" mh-folder-title))
	(write-named-file (concat mh-buffer-filename "/"
				  (&mh-header-file-name)))
	(unlink-file (concat mh-buffer-filename "/++"))
    )
    (&mh-header-file-name
	(if (!= (substr (current-buffer-name) 2 1) "/")
	    (current-buffer-name)
	    (save-excursion x
		   (setq x (current-buffer-name))
		   (temp-use-buffer "scratch")
		   (erase-buffer) (insert-string x)
		   (beginning-of-file) (set-mark)
		   (error-occured 
		       (replace-string "/" ".")
		   )
		   (end-of-file)
		   (region-to-string)
	    )
	)
    )
)
; Read in the ++ file that was generated by an external "inc", then erase.
(defun 
    (&mh-update-headers uhf
	(setq uhf (concat mh-buffer-filename "/++"))
	(if (file-exists uhf)
	    (progn 
		   (save-excursion
		       (temp-use-buffer "++")
		       (read-file uhf)
		       (temp-use-buffer (concat "+" mh-folder))
		       (end-of-file)
		       (yank-buffer "++")
		       (write-current-file)
		       (temp-use-buffer "++")
		       (erase-buffer)
		       (unlink-file uhf)
		   )
	    )
	)
    )
)
; This function removes all "+" flags from the headers, and if it is called
; with an argument of 1, removes all "D" and "^" flags too.
(defun 
    (&mh-unmark-all-headers
	(temp-use-buffer (concat "+" mh-folder))
	(beginning-of-file)
	(while (! (error-occured
		      (if (= 0 (arg 1))
			  (re-search-forward "^...\\+")
			  (re-search-forward "^...\\D\\|^...\\^\\|^...\\+")
		      )
		  )
	       )
	       (delete-previous-character)
	       (insert-character ' ')
	)
    )
    
; position the cursor to the current message.
    (&mh-position-to-current curmsg curbuf curfil
	(setq curbuf (current-buffer-name))
	(setq curfil mh-buffer-filename)
	(temp-use-buffer "mh-temp") (erase-buffer)
	(if (error-occured 
		(insert-file (concat curfil "/cur")))
	    (setq curmsg 0)
	    (progn
		  (beginning-of-file)
		  (set-mark)
		  (end-of-line)
		  (setq curmsg (region-to-string))
	    )
	)
	(temp-use-buffer curbuf)
	(end-of-file)
	(if (= curmsg 0) (previous-line)
	    (progn
		  (while (< (length curmsg) 3)
			 (setq curmsg (concat " " curmsg)))
		  (if (error-occured 
			  (re-search-reverse (concat "^" curmsg)))
		      (progn (end-of-file) (previous-line))
		  )
	    )
	)
	(if (! (eobp))
	    (progn
		  (beginning-of-line)
		  (goto-character (+ (dot) 3))
		  (delete-next-character)
		  (insert-character '+')
		  (beginning-of-line)
	    )
	)
    )
; This function sets the "current message" (+ sign) to equal the number of
; the message that the cursor is pointing to. I.e. it writes cur to stable
; storage
    (&mh-set-cur cm cf
	(save-window-excursion 
	    (temp-use-buffer (concat "+" mh-folder))
	    (setq cm (&mh-get-msgnum))
	    (setq cf (concat mh-buffer-filename "/cur"))
	    (temp-use-buffer "mh-temp")
	    (erase-buffer)
	    (insert-string cm)
	    (write-named-file cf)
	    (delete-buffer "mh-temp")
	)
    )
    
; write out the header buffer as a file in the current folder
    (&mh-make-headers-current
	(temp-use-buffer (concat "+" mh-folder))
	(save-excursion 
	    (beginning-of-file)
	    (while (! (error-occured
			  (re-search-forward "^...\\D\\|^...\\^")))
		   (beginning-of-line)
		   (kill-to-end-of-line) (delete-next-character)
	    )
	    (write-current-file)
	)
	(&mh-set-cur)
    )

; This function closes a folder, i.e. processes all of the pending deletes and
    ; moves for it and edits the header buffer accordingly.
    (&mh-close-folder ts
	(temp-use-buffer "cmd-buffer") (beginning-of-file)
	(error-occured 
	    (re-search-forward (concat "^rmm +" mh-folder))
	    (beginning-of-line) (insert-string mh-progs "/")
	    (beginning-of-line) (set-mark)
	    (end-of-line) (delete-next-character)
	    (setq ts (region-to-string)) (erase-region)
	    (send-to-shell ts)
	)
	(beginning-of-file)
	(while (! 
		  (error-occured
		      (re-search-forward (concat "^filem -src +" mh-folder))
		  ))
		  (beginning-of-line) (insert-string mh-progs "/")
		  (beginning-of-line) (set-mark)
		  (end-of-line) (delete-next-character)
		  (setq ts (region-to-string)) (erase-region)
		  (send-to-shell ts)
	)
	(pop-to-buffer (concat "+" mh-folder))
	(&mh-make-headers-current)
	(&mh-unmark-all-headers)
	(&mh-position-to-current)
    )
;  This function applies "folder -pack" to the current folder, after first
;  closing it (see above)
    (&mh-pack-folder sm
	(setq sm mode-line-format)
	(setq mode-line-format " closing folder first...") (sit-for 0)
	(&mh-close-folder)
	(setq mode-line-format " please wait for pack...") (sit-for 0)
	(send-to-shell (concat mh-progs "/folder +" mh-folder " -pack"))
	(&mh-regenerate-headers)
	(setq mode-line-format sm)
    )
)
