<br/>
<div class='titlediv'>
  <h1 class='newstitle'> 
   __Move media__
  </h1>
 </div>
 <div class='bodydiv'>

<form action="?" method='GET'>
<table>
<tr>
<td><b>__To:__ </b></td><td><input class='formulaire' name='email' value='<TMPL_VAR email>'></td>
</tr><tr>
<td><b>__Subject:__ </b></td><td><input class='formulaire' name='subject' value='__[BACULA] Move media to__ <TMPL_VAR newlocation>' size='80'></td>
</tr><tr>
<td></td>
<td>
<textarea name='content' class='formulaire' cols='80' rows='32'>
__Hi,__

__Could you move these media to__ <TMPL_VAR newlocation>
Media :
<TMPL_LOOP media>
 - <TMPL_VAR VolumeName>  (<TMPL_VAR location>)
</TMPL_LOOP>

__When it's finish, could you update media location?__
(__you can use this link:__ <TMPL_VAR url>).

__Thanks__
</textarea>
</td></tr></table>
<input class='formulaire' type='submit' name='action' value='move_email'>
</form>
<br>
<a href="<TMPL_VAR url>"><img alt='__Update now__' src='/bweb/update.png'>__Update now__</a>
</div>
