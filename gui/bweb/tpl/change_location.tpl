<br/>
<div class='titlediv'>
  <h1 class='newstitle'> 
   Move media
  </h1>
 </div>
 <div class='bodydiv'>

<form action="?" method='GET'>
<table>
<tr>
<td><b>To: </b></td><td><input class='formulaire' name='email' value='<TMPL_VAR email>'></td>
</tr><tr>
<td><b>Subject: </b></td><td><input class='formulaire' name='subject' value='[BACULA] Move media to <TMPL_VAR newlocation>' size='80'></td>
</tr><tr>
<td></td>
<td>
<textarea name='content' class='formulaire' cols='80' rows='32'>
Hi,

Could you move these media to <TMPL_VAR newlocation>
Media :
<TMPL_LOOP Medias>
 - <TMPL_VAR VolumeName>  (<TMPL_VAR location>)
</TMPL_LOOP>

When it's finish, could you update media location ? (you can use <a
href="<TMPL_VAR url>">
this link</a>).

Thanks
</textarea>
</td></tr></table>
<input class='formulaire' type='submit' name='action' value='email_move'>
</form>
<br>
<a href="<TMPL_VAR url>"><img alt='update now' src='/bweb/update.png'>Update now</a>
</div>
