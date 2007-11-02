<div class='titlediv'>
  <h1 class='newstitle'> User: <TMPL_VAR username></h1>
</div>
<div class='bodydiv'>

<form name="form1" action="?">
<input type="hidden" value="1" name="create">
 <table>
 <tr>
  <td>Username:</td> <td> <input class="formulaire" type="text" name="username" value="<TMPL_VAR username>"> </td>
<!-- </tr><tr>
  <td>Password:</td> <td> <input class="formulaire" type="password" name="passwd" value="<TMPL_VAR passwd>"> </td>
-->
 </tr><tr>
  <td>Comment:</td> <td> <input class="formulaire" type="text" name="comment" value="<TMPL_VAR comment>"> </td>
 </tr><tr>
<td> Profile:</td><td>
 <select name="profile" id='profile' class="formulaire">
  <option onclick='set_role("")'></option>
  <option onclick='set_role("administrator")'>Administrator</option>
  <option onclick='set_role("customer")'>Customer</option>
 </select>
</td><td>Or like an existing user: </td><td>
 <select name="copy_username" class="formulaire">
  <option onclick="disable_sel(false)"></option>
 <TMPL_LOOP db_usernames>
  <option title="<TMPL_VAR comment>" onclick="disable_sel(true)" value="<TMPL_VAR username>"><TMPL_VAR username></option>
 </TMPL_LOOP>
 </select>
</td>
 </tr><tr>
 </tr><tr>
<td> Roles:</td><td>
 <select name="rolename" id='rolename' multiple class="formulaire" size=15>
 <TMPL_LOOP db_roles>
  <option title="<TMPL_VAR comment>" value="<TMPL_VAR rolename>" <TMPL_IF userid>selected</TMPL_IF> ><TMPL_VAR rolename></option>
 </TMPL_LOOP>
 </select>
 </td>
</table>
    <input type="image" name='action' value='user_save'
     src='/bweb/save.png'>
</form>
</div>

<script type="text/javascript" language='JavaScript'>
function disable_sel(val) 
{
	document.form1.profile.disabled = val;
	document.form1.rolename.disabled = val;
}
function set_role(val)
{
   if (val == "administrator") {
	for (var i=0; i < document.form1.rolename.length; ++i) {
	      document.form1.rolename[i].selected = true;
	}
   } else if (val == "customer") {
	for (var i=0; i < document.form1.rolename.length; ++i) {
	   if (document.form1.rolename[i].value == 'view_stats'   ||
               document.form1.rolename[i].value == 'view_history' ||
               document.form1.rolename[i].value == 'view_log'
               )
           {
	      document.form1.rolename[i].selected = true;
	   } else {
	      document.form1.rolename[i].selected = false;
	   }
	}
   }

}
</script>
